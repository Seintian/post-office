/**
 * @file logstore_worker.c
 * @brief Flush worker & background fsync threads.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include "log/logger.h"
#include "metrics/metrics.h"
#include "storage/logstore_internal.h"

uint64_t _ls_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

// Worker thread: drains batched append requests and persists them.
void *_ls_worker_main(void *arg) {
    po_logstore_t *ls = (po_logstore_t *)arg;
    void **batch = malloc(sizeof(void *) * ls->batch_size);
    if (!batch)
        return NULL;

    PO_METRIC_TIMER_CREATE("logstore.flush.ns");
    static const uint64_t _flush_bins[] = {1000,5000,10000,50000,100000,500000,1000000,5000000,10000000};
    PO_METRIC_HISTO_CREATE("logstore.flush.latency", _flush_bins, sizeof(_flush_bins)/sizeof(_flush_bins[0]));

    for (;;) {
        ssize_t n = perf_batcher_next(ls->b, batch);
        if (n < 0) {
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 1 * 1000 * 1000};
            nanosleep(&ts, NULL);
            if (!atomic_load(&ls->running) && perf_ringbuf_count(ls->q) == 0)
                break;
            continue;
        }
        if (n == 0) {
            if (!atomic_load(&ls->running) && perf_ringbuf_count(ls->q) == 0)
                break;
            continue; // spurious wake
        }
    PO_METRIC_COUNTER_INC("logstore.flush.batch_count");
    PO_METRIC_COUNTER_ADD("logstore.flush.batch_records", (uint64_t)n);
    PO_METRIC_TIMER_START("logstore.flush.ns");
    PO_METRIC_TICK(_flush_start);
        if (n == 1) {
            append_req_t *only = (append_req_t *)batch[0];
            if (only && only->k == NULL && only->v == NULL) {
                free(only); // sentinel
                ls->sentinel = NULL;
                if (!atomic_load(&ls->running) && perf_ringbuf_count(ls->q) == 0)
                    break;
                else {
                    PO_METRIC_TIMER_STOP("logstore.flush.ns");
                    PO_METRIC_HISTO_RECORD("logstore.flush.latency", 0);
                    continue;
                }
            }
        }

        // Count live records (skip sentinel if present)
        int sentinel_seen = 0;
        ssize_t live = 0;
        for (ssize_t i = 0; i < n; ++i) {
            append_req_t *req = (append_req_t *)batch[i];
            if (req == ls->sentinel) {
                sentinel_seen = 1;
                continue;
            }
            live++;
        }
        if (live == 0 && sentinel_seen) {
            if (ls->sentinel) {
                free(ls->sentinel);
                ls->sentinel = NULL;
            }
            if (!atomic_load(&ls->running) && perf_ringbuf_count(ls->q) == 0)
                break;
            continue;
        }

        off_t base = lseek(ls->fd, 0, SEEK_END);
        if (base < 0) {
            LOG_ERROR("logstore: lseek failed");
            base = 0;
        }

        size_t iov_cnt = (size_t)live * 4u;
        struct iovec *iov = (struct iovec *)malloc(sizeof(struct iovec) * iov_cnt);
        uint64_t *offs = (uint64_t *)malloc(sizeof(uint64_t) * (size_t)live);
        uint32_t *lens = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)live);
        uint32_t *kls_arr = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)live);
        if (!iov || !offs || !lens || !kls_arr) {
            // Fallback single write path (duplicated intentionally for isolation)
            for (ssize_t i = 0; i < n; ++i) {
                append_req_t *req = (append_req_t *)batch[i];
                if (req == ls->sentinel)
                    continue;
                uint32_t kl = (uint32_t)req->klen, vl = (uint32_t)req->vlen;
                off_t off = lseek(ls->fd, 0, SEEK_END);
                if (off < 0)
                    off = 0;
                if (write(ls->fd, &kl, sizeof(kl)) != (ssize_t)sizeof(kl))
                    LOG_ERROR("logstore: write kl failed");
                if (write(ls->fd, &vl, sizeof(vl)) != (ssize_t)sizeof(vl))
                    LOG_ERROR("logstore: write vl failed");
                if (write(ls->fd, req->k, req->klen) != (ssize_t)req->klen)
                    LOG_ERROR("logstore: write key failed");
                if (write(ls->fd, req->v, req->vlen) != (ssize_t)req->vlen)
                    LOG_ERROR("logstore: write val failed");
                uint8_t iv[12];
                memcpy(iv, &off, 8);
                memcpy(iv + 8, &vl, 4);
                (void)db_put(ls->idx, req->k, req->klen, iv, sizeof iv);
                pthread_rwlock_wrlock(&ls->idx_lock);
                (void)po_index_put(ls->mem_idx, req->k, req->klen, (uint64_t)off, vl);
                pthread_rwlock_unlock(&ls->idx_lock);
                free(req); // single allocation
                atomic_fetch_sub(&ls->outstanding_reqs, 1);
            }
            if (ls->fsync_policy == PO_LS_FSYNC_EACH_BATCH)
                fsync(ls->fd);
            goto post_batch;
        }

        uint64_t cur = (uint64_t)base;
        size_t j = 0;
        size_t rec_index = 0;
        for (ssize_t i = 0; i < n; ++i) {
            append_req_t *req = (append_req_t *)batch[i];
            if (req == ls->sentinel)
                continue;
            uint32_t vl = (uint32_t)req->vlen;
            kls_arr[rec_index] = (uint32_t)req->klen;
            lens[rec_index] = vl;
            offs[rec_index] = cur;
            iov[j++] = (struct iovec){.iov_base = &kls_arr[rec_index], .iov_len = sizeof(uint32_t)};
            cur += sizeof(uint32_t);
            iov[j++] = (struct iovec){.iov_base = &lens[rec_index], .iov_len = sizeof(uint32_t)};
            cur += sizeof(uint32_t);
            iov[j++] = (struct iovec){.iov_base = req->k, .iov_len = req->klen};
            cur += (uint64_t)req->klen;
            iov[j++] = (struct iovec){.iov_base = req->v, .iov_len = req->vlen};
            cur += (uint64_t)req->vlen;
            rec_index++;
        }
        ssize_t w = pwritev(ls->fd, iov, (int)iov_cnt, base);
        if (w < 0) {
            LOG_ERROR("logstore: pwritev failed");
        } else {
            rec_index = 0;
            for (ssize_t i = 0; i < n; ++i) {
                append_req_t *req = (append_req_t *)batch[i];
                if (req == ls->sentinel)
                    continue;
                uint8_t iv[12];
                memcpy(iv, &offs[rec_index], 8);
                memcpy(iv + 8, &lens[rec_index], 4);
                (void)db_put(ls->idx, req->k, req->klen, iv, sizeof iv);
                pthread_rwlock_wrlock(&ls->idx_lock);
                (void)po_index_put(ls->mem_idx, req->k, req->klen, offs[rec_index],
                                   lens[rec_index]);
                pthread_rwlock_unlock(&ls->idx_lock);
                rec_index++;
            }
            if (ls->fsync_policy == PO_LS_FSYNC_EACH_BATCH)
                fsync(ls->fd);
            else if (ls->fsync_policy == PO_LS_FSYNC_EVERY_N) {
                ls->batches_since_fsync++;
                if (ls->batches_since_fsync >= (ls->fsync_every_n ? ls->fsync_every_n : 1)) {
                    (void)fsync(ls->fd);
                    ls->batches_since_fsync = 0;
                }
            } else if (ls->fsync_policy == PO_LS_FSYNC_INTERVAL) {
                uint64_t now = _ls_now_ns();
                if (now - ls->last_fsync_ns >= ls->fsync_interval_ns) {
                    (void)fsync(ls->fd);
                    ls->last_fsync_ns = now;
                }
            }
        }
        for (ssize_t i = 0; i < n; ++i) {
            append_req_t *req = (append_req_t *)batch[i];
            if (req == ls->sentinel)
                continue;
            free(req);
            atomic_fetch_sub(&ls->outstanding_reqs, 1);
        }
        atomic_fetch_add(&ls->metric_records_flushed, (uint64_t)live);
        atomic_fetch_add(&ls->metric_batches_flushed, 1);
    PO_METRIC_COUNTER_ADD("logstore.flush.records", (uint64_t)live);
    PO_METRIC_COUNTER_INC("logstore.flush.batch_count");
    PO_METRIC_TIMER_STOP("logstore.flush.ns");
    uint64_t elapsed = PO_METRIC_ELAPSED_NS(_flush_start);
    PO_METRIC_HISTO_RECORD("logstore.flush.latency", elapsed);
        if (sentinel_seen && ls->sentinel) {
            free(ls->sentinel);
            ls->sentinel = NULL;
        }
        free(iov);
        free(offs);
        free(lens);
        free(kls_arr);
    post_batch:
        if (!atomic_load(&ls->running) && perf_ringbuf_count(ls->q) == 0 && ls->sentinel == NULL) {
            // Final defensive drain (paranoia)
            void *left = NULL;
            while (perf_ringbuf_dequeue(ls->q, &left) == 0) {
                append_req_t *req = (append_req_t *)left;
                if (!req)
                    continue;
                if (req == ls->sentinel) {
                    free(req);
                    ls->sentinel = NULL;
                    continue;
                }
                free(req);
                atomic_fetch_sub(&ls->outstanding_reqs, 1);
            }
            break;
        }
    }
    free(batch);
    return NULL;
}

// Background fsync thread for interval policy.
void *_ls_fsync_thread_main(void *arg) {
    po_logstore_t *ls = (po_logstore_t *)arg;
    const uint64_t interval = ls->fsync_interval_ns ? ls->fsync_interval_ns : 50 * 1000000ull;
    while (atomic_load(&ls->fsync_thread_run)) {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = (long)interval};
        nanosleep(&ts, NULL);
        if (!atomic_load(&ls->running)) {
            (void)fsync(ls->fd);
            break;
        }
        (void)fsync(ls->fd);
    }
    return NULL;
}
