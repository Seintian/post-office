/**
 * @file logstore.c
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "storage/logstore.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <time.h>

#include "log/logger.h"
#include "perf/ringbuf.h"
#include "perf/batcher.h"
#include "storage/db_lmdb.h"
#include "storage/index.h"

// --- Configuration Defaults & Internal Helpers ---------------------------------
// These can be tuned via the public configuration passed to po_logstore_open_cfg.
#ifndef LS_MAX_KEY_DEFAULT
#define LS_MAX_KEY_DEFAULT   (1024u * 1024u)      /* 1 MiB */
#endif
#ifndef LS_MAX_VALUE_DEFAULT
#define LS_MAX_VALUE_DEFAULT (8u * 1024u * 1024u) /* 8 MiB */
#endif

#define LS_HARD_KEY_MAX   (32u * 1024u * 1024u)   /* 32 MiB absolute upper bound (corruption guard) */
#define LS_HARD_VALUE_MAX (128u * 1024u * 1024u)  /* 128 MiB absolute upper bound (corruption guard) */

// Validate key/value lengths against configured and hard limits. Returns 0 if ok, -1 otherwise.
static inline int _ls_validate_lengths(size_t klen, size_t vlen, size_t maxk, size_t maxv) {
    if (klen == 0) return -1;                // empty keys not allowed
    if (klen > maxk) return -1;              // over configured limit
    if (vlen > maxv) return -1;              // over configured limit
    if (klen > LS_HARD_KEY_MAX) return -1;   // guard against unreasonable values
    if (vlen > LS_HARD_VALUE_MAX) return -1; // guard against unreasonable values
    return 0;
}

typedef struct {
    void *k;
    size_t klen;
    void *v;
    size_t vlen;
} append_req_t;

struct po_logstore {
    int fd; // append-only file descriptor
    db_env_t *env;
    db_bucket_t *idx; // LMDB bucket used as index key -> (offset,len) packed
    perf_ringbuf_t *q;
    perf_batcher_t *b;
    pthread_t worker;
    _Atomic int running;
    // fast-path in-memory index guarded by RW lock
    po_index_t *mem_idx;
    pthread_rwlock_t idx_lock;
    size_t batch_size;
    po_logstore_fsync_policy_t fsync_policy;
    _Atomic uint64_t seq;
    append_req_t *sentinel; // track sentinel for leak-free shutdown
    uint64_t fsync_interval_ns; // interval in ns (0 if unused)
    uint64_t last_fsync_ns;     // last fsync time (CLOCK_REALTIME ns)
    unsigned fsync_every_n;     // if policy == EVERY_N: threshold
    unsigned batches_since_fsync; // counter for EVERY_N
    int background_fsync; // flag
    pthread_t fsync_thread; // optional background fsync thread
    _Atomic int fsync_thread_run;
    size_t max_key_bytes;   // configured maximum key size
    size_t max_value_bytes; // configured maximum value size
};

static uint64_t _now_ns(void) {
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void *worker_main(void *arg) {
    po_logstore_t *ls = (po_logstore_t *)arg;
    void **batch = malloc(sizeof(void*) * ls->batch_size);
    if (!batch) return NULL;
    for (;;) {
        ssize_t n = perf_batcher_next(ls->b, batch);
        if (n < 0) {
            // transient error; idle
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 1 * 1000 * 1000 };
            nanosleep(&ts, NULL);
            if (!atomic_load(&ls->running) && perf_ringbuf_count(ls->q) == 0) break;
            continue;
        }
        if (n == 1) {
            append_req_t *only = (append_req_t *)batch[0];
            if (only && only->k == NULL && only->v == NULL) {
                free(only);
                ls->sentinel = NULL;
                if (!atomic_load(&ls->running) && perf_ringbuf_count(ls->q) == 0) {
                    break; // clean shutdown
                } else {
                    continue; // more items pending
                }
            }
        }
        // normal batch processing even if running flag cleared (drain remaining)
        // compute base offset and build iovecs (skipping sentinel if present)
        int sentinel_seen = 0;
        ssize_t live = 0;
        for (ssize_t i = 0; i < n; ++i) {
            append_req_t *req = (append_req_t *)batch[i];
            if (req == ls->sentinel) { sentinel_seen = 1; continue; }
            live++;
        }
        if (live == 0 && sentinel_seen) {
            // sentinel only (should have been handled by early path but be safe)
            if (ls->sentinel) { free(ls->sentinel); ls->sentinel = NULL; }
            if (!atomic_load(&ls->running) && perf_ringbuf_count(ls->q) == 0) break;
            continue;
        }
        off_t base = lseek(ls->fd, 0, SEEK_END);
        if (base < 0) { LOG_ERROR("logstore: lseek failed"); base = 0; }
        size_t iov_cnt = (size_t)live * 4u;
        struct iovec *iov = (struct iovec *)malloc(sizeof(struct iovec) * iov_cnt);
        if (!iov) {
            // fallback: process one by one
            for (ssize_t i = 0; i < n; ++i) {
                append_req_t *req = (append_req_t *)batch[i];
                if (req == ls->sentinel) { continue; }
                uint32_t kl = (uint32_t)req->klen, vl = (uint32_t)req->vlen;
                off_t off = lseek(ls->fd, 0, SEEK_END);
                if (off < 0) off = 0;
                ssize_t wr;
                wr = write(ls->fd, &kl, sizeof(kl)); if (wr != (ssize_t)sizeof(kl)) { LOG_ERROR("logstore: write kl failed"); }
                wr = write(ls->fd, &vl, sizeof(vl)); if (wr != (ssize_t)sizeof(vl)) { LOG_ERROR("logstore: write vl failed"); }
                wr = write(ls->fd, req->k, req->klen); if (wr != (ssize_t)req->klen) { LOG_ERROR("logstore: write key failed"); }
                wr = write(ls->fd, req->v, req->vlen); if (wr != (ssize_t)req->vlen) { LOG_ERROR("logstore: write val failed"); }
                uint8_t iv[12];
                memcpy(iv, &off, 8); memcpy(iv + 8, &vl, 4);
                (void)db_put(ls->idx, req->k, req->klen, iv, sizeof iv);
                pthread_rwlock_wrlock(&ls->idx_lock);
                (void)po_index_put(ls->mem_idx, req->k, req->klen, (uint64_t)off, vl);
                pthread_rwlock_unlock(&ls->idx_lock);
                free(req->k); free(req->v); free(req);
            }
            if (ls->fsync_policy == PO_LS_FSYNC_EACH_BATCH) fsync(ls->fd);
            continue;
        }
        // fill iovecs and compute offsets for index updates
        uint64_t cur = (uint64_t)base;
    uint64_t *offs = (uint64_t *)malloc(sizeof(uint64_t) * (size_t)live);
    uint32_t *lens = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)live); // value lengths
    uint32_t *kls_arr = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)live); // key lengths
        if (!offs || !lens || !kls_arr) {
            free(iov); free(offs); free(lens); free(kls_arr);
            // fallback single as above
            for (ssize_t i = 0; i < n; ++i) {
                append_req_t *req = (append_req_t *)batch[i];
        if (req == ls->sentinel) { continue; }
                uint32_t kl = (uint32_t)req->klen, vl = (uint32_t)req->vlen;
                off_t off = lseek(ls->fd, 0, SEEK_END);
                if (off < 0) off = 0;
                ssize_t wr;
                wr = write(ls->fd, &kl, sizeof(kl)); if (wr != (ssize_t)sizeof(kl)) { LOG_ERROR("logstore: write kl failed"); }
                wr = write(ls->fd, &vl, sizeof(vl)); if (wr != (ssize_t)sizeof(vl)) { LOG_ERROR("logstore: write vl failed"); }
                wr = write(ls->fd, req->k, req->klen); if (wr != (ssize_t)req->klen) { LOG_ERROR("logstore: write key failed"); }
                wr = write(ls->fd, req->v, req->vlen); if (wr != (ssize_t)req->vlen) { LOG_ERROR("logstore: write val failed"); }
                uint8_t iv[12];
                memcpy(iv, &off, 8); memcpy(iv + 8, &vl, 4);
                (void)db_put(ls->idx, req->k, req->klen, iv, sizeof iv);
                pthread_rwlock_wrlock(&ls->idx_lock);
                (void)po_index_put(ls->mem_idx, req->k, req->klen, (uint64_t)off, vl);
                pthread_rwlock_unlock(&ls->idx_lock);
                free(req->k); free(req->v); free(req);
            }
            if (ls->fsync_policy == PO_LS_FSYNC_EACH_BATCH) fsync(ls->fd);
            continue;
        }
        size_t j = 0; size_t rec_index = 0;
        for (ssize_t i = 0; i < n; ++i) {
            append_req_t *req = (append_req_t *)batch[i];
            if (req == ls->sentinel) continue;
            uint32_t vl = (uint32_t)req->vlen;
            kls_arr[rec_index] = (uint32_t)req->klen;
            lens[rec_index] = vl;
            offs[rec_index] = cur;
            iov[j++] = (struct iovec){ .iov_base = &kls_arr[rec_index], .iov_len = sizeof(uint32_t) };
            cur += sizeof(uint32_t);
            iov[j++] = (struct iovec){ .iov_base = &lens[rec_index], .iov_len = sizeof(uint32_t) };
            cur += sizeof(uint32_t);
            iov[j++] = (struct iovec){ .iov_base = req->k, .iov_len = req->klen };
            cur += (uint64_t)req->klen;
            iov[j++] = (struct iovec){ .iov_base = req->v, .iov_len = req->vlen };
            cur += (uint64_t)req->vlen;
            rec_index++;
        }
        ssize_t w = pwritev(ls->fd, iov, (int)iov_cnt, base);
        if (w < 0) {
            LOG_ERROR("logstore: pwritev failed");
        } else {
            // index updates
            rec_index = 0;
            for (ssize_t i = 0; i < n; ++i) {
                append_req_t *req = (append_req_t *)batch[i];
                if (req == ls->sentinel) continue;
                uint8_t iv[12];
                memcpy(iv, &offs[rec_index], 8); memcpy(iv + 8, &lens[rec_index], 4);
                (void)db_put(ls->idx, req->k, req->klen, iv, sizeof iv);
                pthread_rwlock_wrlock(&ls->idx_lock);
                (void)po_index_put(ls->mem_idx, req->k, req->klen, offs[rec_index], lens[rec_index]);
                pthread_rwlock_unlock(&ls->idx_lock);
                rec_index++;
            }
            if (ls->fsync_policy == PO_LS_FSYNC_EACH_BATCH) {
                (void)fsync(ls->fd);
            } else if (ls->fsync_policy == PO_LS_FSYNC_EVERY_N) {
                ls->batches_since_fsync++;
                if (ls->batches_since_fsync >= (ls->fsync_every_n ? ls->fsync_every_n : 1)) {
                    (void)fsync(ls->fd);
                    ls->batches_since_fsync = 0;
                }
            } else if (ls->fsync_policy == PO_LS_FSYNC_INTERVAL) {
                uint64_t now = _now_ns();
                if (now - ls->last_fsync_ns >= ls->fsync_interval_ns) {
                    (void)fsync(ls->fd);
                    ls->last_fsync_ns = now;
                }
            }
        }
        for (ssize_t i = 0; i < n; ++i) {
            append_req_t *req = (append_req_t *)batch[i];
            if (req == ls->sentinel) continue; // free after loop
            if (req) { free(req->k); free(req->v); free(req); }
        }
        if (sentinel_seen && ls->sentinel) { free(ls->sentinel); ls->sentinel = NULL; }
    free(iov); free(offs); free(lens); free(kls_arr);
    }
    free(batch);
return NULL;
}

static void *fsync_thread_main(void *arg) {
    po_logstore_t *ls = (po_logstore_t*)arg;
    const uint64_t interval = ls->fsync_interval_ns ? ls->fsync_interval_ns : 50*1000000ull; // fallback 50ms
    while (atomic_load(&ls->fsync_thread_run)) {
        struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = (long)(interval);
        nanosleep(&ts, NULL);
        if (!atomic_load(&ls->running)) {
            // still flush pending writes before exit
            (void)fsync(ls->fd);
            break;
        }
        (void)fsync(ls->fd);
    }
    return NULL;
}

struct preload_ud { po_index_t *idx; };
static int preload_cb(const void *k, size_t klen, const void *v, size_t vlen, void *ud) {
    (void)ud;
    if (vlen == 12) {
        uint64_t off;
        uint32_t len;
        memcpy(&off, v, 8);
        memcpy(&len, (const uint8_t *)v + 8, 4);
        (void)po_index_put(((struct preload_ud *)ud)->idx, k, klen, off, len);
    }
    return 0;
}

po_logstore_t *po_logstore_open_cfg(const po_logstore_cfg *cfg) {
    if (!cfg || !cfg->dir || !cfg->bucket) return NULL;
    char path[512];
    snprintf(path, sizeof(path), "%s/aof.log", cfg->dir);
    int fd = open(path, O_CREAT | O_RDWR, 0664); // need read for pread()
    if (fd < 0) {
        LOG_ERROR("logstore: open(%s) failed", path);
        return NULL;
    }

    db_env_t *env = NULL;
    if (db_env_open(cfg->dir, 8, cfg->map_size, &env) != 0) {
        LOG_ERROR("logstore: db_env_open failed");
        close(fd);
        return NULL;
    }
    db_bucket_t *idx = NULL;
    if (db_bucket_open(env, cfg->bucket, &idx) != 0) {
        LOG_ERROR("logstore: db_bucket_open(%s) failed", cfg->bucket);
        db_env_close(&env);
        close(fd);
        return NULL;
    }

    po_logstore_t *ls = (po_logstore_t *)calloc(1, sizeof(*ls));
    if (!ls) { db_bucket_close(&idx); db_env_close(&env); close(fd); return NULL; }
    ls->fd = fd;
    ls->env = env;
    ls->idx = idx;
    size_t cap = cfg->ring_capacity ? cfg->ring_capacity : 1024;
    ls->q = perf_ringbuf_create(cap);
    if (!ls->q) { po_logstore_close(&ls); return NULL; }
    ls->b = perf_batcher_create(ls->q, cfg->batch_size ? cfg->batch_size : 32);
    if (!ls->b) { po_logstore_close(&ls); return NULL; }
    ls->mem_idx = po_index_create(1024);
    if (!ls->mem_idx) { po_logstore_close(&ls); return NULL; }
    pthread_rwlock_init(&ls->idx_lock, NULL);
    ls->batch_size = cfg->batch_size ? cfg->batch_size : 32;
    ls->fsync_policy = cfg->fsync_policy;
    atomic_store(&ls->seq, 0);
    // preload in-memory index from LMDB to speed up reads
    struct preload_ud u = { .idx = ls->mem_idx };
    (void)db_iterate(ls->idx, preload_cb, &u);
    atomic_store(&ls->running, 1);
    if (ls->fsync_policy == PO_LS_FSYNC_INTERVAL && cfg->fsync_interval_ms > 0) {
        ls->fsync_interval_ns = (uint64_t)cfg->fsync_interval_ms * 1000000ull;
        ls->last_fsync_ns = _now_ns();
    } else {
        ls->fsync_interval_ns = 0; ls->last_fsync_ns = 0;
    }
    if (ls->fsync_policy == PO_LS_FSYNC_EVERY_N) {
        ls->fsync_every_n = cfg->fsync_every_n ? cfg->fsync_every_n : 1;
        ls->batches_since_fsync = 0;
    } else {
        ls->fsync_every_n = 0; ls->batches_since_fsync = 0;
    }

    // Configure max key/value sizes (fallback to defaults if zero supplied)
    ls->max_key_bytes = cfg->max_key_bytes ? cfg->max_key_bytes : LS_MAX_KEY_DEFAULT;
    ls->max_value_bytes = cfg->max_value_bytes ? cfg->max_value_bytes : LS_MAX_VALUE_DEFAULT;
    // Enforce that configured values never exceed hard bounds to prevent wrap/overflow scenarios.
    if (ls->max_key_bytes > LS_HARD_KEY_MAX) ls->max_key_bytes = LS_HARD_KEY_MAX;
    if (ls->max_value_bytes > LS_HARD_VALUE_MAX) ls->max_value_bytes = LS_HARD_VALUE_MAX;

    // Optional rebuild-on-open: scan file sequentially and rebuild LMDB + memory index
    if (cfg->rebuild_on_open) {
        off_t cursor = 0; off_t last_good_end = 0;
        for (;;) {
            uint32_t kl = 0, vl = 0;
            ssize_t rd = pread(ls->fd, &kl, sizeof(kl), cursor);
            if (rd == 0) { // EOF clean
                last_good_end = cursor; break; }
            if (rd != (ssize_t)sizeof(kl)) { break; }
            rd = pread(ls->fd, &vl, sizeof(vl), cursor + (off_t)sizeof(kl));
            if (rd != (ssize_t)sizeof(vl)) { break; }
            // Validate lengths using both configured and absolute hard bounds.
            if (_ls_validate_lengths(kl, vl, ls->max_key_bytes, ls->max_value_bytes) != 0) {
                break; // corruption / out-of-range -> stop
            }
            size_t rec_sz = sizeof(uint32_t)*2 + (size_t)kl + (size_t)vl;
            void *kbuf = malloc(kl);
            if (!kbuf) break;
            rd = pread(ls->fd, kbuf, kl, cursor + (off_t)sizeof(uint32_t)*2);
            if (rd != (ssize_t)kl) { free(kbuf); break; }
            // Rigorous verification: ensure value region fully present
            if (vl > 0) {
                char probe;
                rd = pread(ls->fd, &probe, 1, cursor + (off_t)sizeof(uint32_t)*2 + kl + vl - 1);
                if (rd != 1) { free(kbuf); break; }
            }
            uint8_t iv[12]; memcpy(iv, &cursor, 8); memcpy(iv+8, &vl, 4);
            (void)db_put(ls->idx, kbuf, kl, iv, sizeof iv);
            pthread_rwlock_wrlock(&ls->idx_lock);
            (void)po_index_put(ls->mem_idx, kbuf, kl, (uint64_t)cursor, vl);
            pthread_rwlock_unlock(&ls->idx_lock);
            free(kbuf);
            cursor += (off_t)rec_sz; last_good_end = cursor;
        }
        if (cfg->truncate_on_rebuild) {
            int _trc = ftruncate(ls->fd, last_good_end); (void)_trc;
        }
    }
    if (pthread_create(&ls->worker, NULL, worker_main, ls) != 0) {
        po_logstore_close(&ls);
        return NULL;
    }
    if (cfg->background_fsync && ls->fsync_policy == PO_LS_FSYNC_INTERVAL && ls->fsync_interval_ns) {
        ls->background_fsync = 1;
        atomic_store(&ls->fsync_thread_run, 1);
        if (pthread_create(&ls->fsync_thread, NULL, fsync_thread_main, ls) != 0) {
            ls->background_fsync = 0; atomic_store(&ls->fsync_thread_run, 0);
        }
    }
    return ls;
}

po_logstore_t *po_logstore_open(const char *dir, const char *bucket, size_t map_size,
                            size_t ring_capacity) {
    po_logstore_cfg cfg = {
        .dir = dir,
        .bucket = bucket,
        .map_size = map_size,
        .ring_capacity = ring_capacity,
        .batch_size = 32,
        .fsync_policy = PO_LS_FSYNC_NONE,
    };
    return po_logstore_open_cfg(&cfg);
}

void po_logstore_close(po_logstore_t **pls) {
    if (!*pls) return;
    po_logstore_t *ls = *pls;
    atomic_store(&ls->running, 0);
    // enqueue sentinel to wake worker if blocked on eventfd
    append_req_t *sent = (append_req_t *)calloc(1, sizeof(*sent));
    if (sent) {
        ls->sentinel = sent;
        (void)perf_batcher_enqueue(ls->b, sent);
    }
    pthread_join(ls->worker, NULL);
    // Additional defensive drain: free any leftover requests still in ring (should be rare)
    for (;;) {
        void *tmp = NULL;
        if (perf_ringbuf_dequeue(ls->q, &tmp) != 0) break;
        append_req_t *req = (append_req_t*)tmp;
        if (req && req != ls->sentinel) { free(req->k); free(req->v); free(req); }
    }
    if (ls->background_fsync) {
        atomic_store(&ls->fsync_thread_run, 0);
        pthread_join(ls->fsync_thread, NULL);
    }
    if (ls->sentinel) { free(ls->sentinel); ls->sentinel = NULL; }
    if (ls->b) perf_batcher_destroy(&ls->b);
    if (ls->q) perf_ringbuf_destroy(&ls->q);
    if (ls->idx) db_bucket_close(&ls->idx);
    if (ls->env) db_env_close(&ls->env);
    if (ls->fd >= 0) close(ls->fd);
    if (ls->mem_idx) po_index_destroy(&ls->mem_idx);
    pthread_rwlock_destroy(&ls->idx_lock);
    free(ls);
    *pls = NULL;
}

int po_logstore_append(po_logstore_t *ls, const void *key, size_t keylen,
                    const void *val, size_t vallen) {
    if (!ls || !key || _ls_validate_lengths(keylen, vallen, ls->max_key_bytes, ls->max_value_bytes) != 0) {
        return -1;
    }
    append_req_t *r = (append_req_t *)calloc(1, sizeof(*r));
    if (!r) return -1;
    r->k = malloc(keylen);
    r->v = malloc(vallen);
    if (!r->k || !r->v) { free(r->k); free(r->v); free(r); return -1; }
    memcpy(r->k, key, keylen);
    memcpy(r->v, val, vallen);
    r->klen = keylen;
    r->vlen = vallen;
    if (perf_batcher_enqueue(ls->b, r) < 0) { free(r->k); free(r->v); free(r); return -1; }
    return 0;
}

int po_logstore_get(po_logstore_t *ls, const void *key, size_t keylen,
                void **out_val, size_t *out_len) {
    if (!ls || !key || keylen == 0 || keylen > ls->max_key_bytes) return -1;
    // fast path: consult in-memory index under read lock
    uint64_t off = 0;
    uint32_t len = 0;
    pthread_rwlock_rdlock(&ls->idx_lock);
    int have = po_index_get(ls->mem_idx, key, keylen, &off, &len);
    pthread_rwlock_unlock(&ls->idx_lock);
    if (have != 0) {
        // fallback to LMDB
        void *tmp = NULL;
        size_t tmplen = 0;
        int rc = db_get(ls->idx, key, keylen, &tmp, &tmplen);
        if (rc != 0) return -1;
        if (tmplen != 12) { free(tmp); return -1; }
        memcpy(&off, tmp, 8);
        memcpy(&len, (uint8_t *)tmp + 8, 4);
        free(tmp);
        // optionally backfill mem index
        pthread_rwlock_wrlock(&ls->idx_lock);
        (void)po_index_put(ls->mem_idx, key, keylen, off, len);
        pthread_rwlock_unlock(&ls->idx_lock);
    }
    // read from file
    uint32_t kl, vl;
    if (pread(ls->fd, &kl, sizeof(kl), (off_t)off) != (ssize_t)sizeof(kl)) return -1;
    if (pread(ls->fd, &vl, sizeof(vl), (off_t)(off + sizeof(kl))) != (ssize_t)sizeof(vl)) return -1;
    if (kl > 16 * 1024 * 1024 || vl != len) return -1;
    void *buf = malloc(vl);
    if (!buf) return -1;
    off_t val_off = (off_t)(off + sizeof(kl) + sizeof(vl) + (off_t)kl);
    if (pread(ls->fd, buf, vl, val_off) != (ssize_t)vl) { free(buf); return -1; }
    *out_val = buf;
    *out_len = vl;
    return 0;
}

// --- Logger integration ---
// Provide a custom sink that appends formatted lines to the logstore
static void _ls_logger_sink(const char *line, void *ud) {
    po_logstore_t *ls = (po_logstore_t *)ud;
    // Key: 16 bytes [ts_ns(8)][seq(8)] as binary; we don't have raw ts,
    // so we use a monotonic seq and current time.
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t ts_ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    uint64_t seq = atomic_fetch_add(&ls->seq, 1);
    uint8_t key[16];
    memcpy(key, &ts_ns, 8); memcpy(key + 8, &seq, 8);
    size_t vlen = strnlen(line, 4096);
    (void)po_logstore_append(ls, key, sizeof key, line, vlen);
}

int po_logstore_attach_logger(po_logstore_t *ls) {
    // this requires logger to expose custom sink API; if unavailable, return -1
    extern int logger_add_sink_custom(void (*fn)(const char*, void*), void *ud);
    return logger_add_sink_custom(_ls_logger_sink, ls);
}

struct scan_ud { po_logstore_t *ls; int prune; po_logstore_integrity_stats *st; off_t end; };
static int _integrity_cb(const void *k, size_t klen, const void *v, size_t vlen, void *pud) {
    struct scan_ud *s = (struct scan_ud*)pud; (void)klen;
    uint64_t off; uint32_t len; uint32_t kl_real; uint32_t vl_real;
    s->st->scanned++;
    if (vlen != 12) { s->st->errors++; return 0; }
    memcpy(&off, v, 8); memcpy(&len, (uint8_t*)v + 8, 4);
    if (off + sizeof(uint32_t)*2 > (uint64_t)s->end) {
        if (s->prune) {
            (void)db_delete(s->ls->idx, k, klen);
            pthread_rwlock_wrlock(&s->ls->idx_lock);
            (void)po_index_remove(s->ls->mem_idx, k, klen);
            pthread_rwlock_unlock(&s->ls->idx_lock);
            s->st->pruned++;
        }
        return 0;
    }
    if (pread(s->ls->fd, &kl_real, sizeof(kl_real), (off_t)off) != (ssize_t)sizeof(kl_real) ||
        pread(s->ls->fd, &vl_real, sizeof(vl_real), (off_t)(off + sizeof(uint32_t))) != (ssize_t)sizeof(vl_real)) {
        s->st->errors++; return 0; }
    if (off + sizeof(uint32_t)*2 + kl_real + vl_real > (uint64_t)s->end) {
        if (s->prune) {
            (void)db_delete(s->ls->idx, k, klen);
            pthread_rwlock_wrlock(&s->ls->idx_lock);
            (void)po_index_remove(s->ls->mem_idx, k, klen);
            pthread_rwlock_unlock(&s->ls->idx_lock);
            s->st->pruned++;
        }
        return 0;
    }
    // Compare stored key bytes with LMDB key (k). Only if lengths match.
    if (kl_real != klen) {
        if (s->prune) {
            (void)db_delete(s->ls->idx, k, klen);
            pthread_rwlock_wrlock(&s->ls->idx_lock);
            (void)po_index_remove(s->ls->mem_idx, k, klen);
            pthread_rwlock_unlock(&s->ls->idx_lock);
            s->st->pruned++;
        }
        return 0;
    }
    void *kbuf = malloc(kl_real);
    if (!kbuf) { s->st->errors++; return 0; }
    if (pread(s->ls->fd, kbuf, kl_real, (off_t)(off + sizeof(uint32_t)*2)) != (ssize_t)kl_real) {
        free(kbuf);
        s->st->errors++; return 0;
    }
    int key_match = (memcmp(kbuf, k, kl_real) == 0);
    free(kbuf);
    if (!key_match) {
        if (s->prune) {
            (void)db_delete(s->ls->idx, k, klen);
            pthread_rwlock_wrlock(&s->ls->idx_lock);
            (void)po_index_remove(s->ls->mem_idx, k, klen);
            pthread_rwlock_unlock(&s->ls->idx_lock);
            s->st->pruned++;
        }
        return 0;
    }
    if (vl_real != len) {
        if (s->prune) {
            (void)db_delete(s->ls->idx, k, klen);
            pthread_rwlock_wrlock(&s->ls->idx_lock);
            (void)po_index_remove(s->ls->mem_idx, k, klen);
            pthread_rwlock_unlock(&s->ls->idx_lock);
            s->st->pruned++;
        }
        return 0;
    }
    s->st->valid++;
    return 0;
}

int po_logstore_integrity_scan(po_logstore_t *ls, int prune_nonexistent,
                               po_logstore_integrity_stats *out_stats) {
    if (!ls) return -1;
    po_logstore_integrity_stats stats = (po_logstore_integrity_stats){0};
    off_t end = lseek(ls->fd, 0, SEEK_END);
    if (end < 0) return -1;
    struct scan_ud ud = { ls, prune_nonexistent, &stats, end };
    (void)db_iterate(ls->idx, _integrity_cb, &ud);
    if (out_stats) *out_stats = stats;
    return 0;
}

int po_logstore_debug_put_index(po_logstore_t *ls, const void *key, size_t keylen,
                                uint64_t offset, uint32_t len) {
    if (!ls || !key || keylen==0) return -1;
    uint8_t iv[12]; memcpy(iv,&offset,8); memcpy(iv+8,&len,4);
    if (db_put(ls->idx, key, keylen, iv, sizeof iv) != 0) return -1;
    pthread_rwlock_wrlock(&ls->idx_lock);
    (void)po_index_put(ls->mem_idx, key, keylen, offset, len);
    pthread_rwlock_unlock(&ls->idx_lock);
    return 0;
}

int po_logstore_debug_lookup(po_logstore_t *ls, const void *key, size_t keylen,
                             uint64_t *out_off, uint32_t *out_len) {
    if (!ls || !key || keylen == 0 || keylen > ls->max_key_bytes || !out_off || !out_len) return -1;
    uint64_t off = 0; uint32_t len = 0;
    pthread_rwlock_rdlock(&ls->idx_lock);
    int have = po_index_get(ls->mem_idx, key, keylen, &off, &len);
    pthread_rwlock_unlock(&ls->idx_lock);
    if (have != 0) {
        void *tmp = NULL; size_t tmplen = 0;
        if (db_get(ls->idx, key, keylen, &tmp, &tmplen) != 0) return -1;
        if (tmplen != 12) { free(tmp); return -1; }
        memcpy(&off, tmp, 8); memcpy(&len, (uint8_t*)tmp + 8, 4);
        free(tmp);
    }
    *out_off = off; *out_len = len; return 0;
}

