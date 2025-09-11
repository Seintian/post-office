/**
 * @file logstore.c
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// Retained file now contains only public API entry points; implementation split
// across smaller translation units for readability. See logstore_*.c.

#include "storage/logstore.h"

#include "storage/logstore_internal.h"
#include "metrics/metrics.h"

// Standard headers needed for open/close & API functions
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Internal dependencies handled via logstore_internal.h
#include "log/logger.h"

// Configuration defaults (tunable via po_logstore_cfg)
#ifndef LS_MAX_KEY_DEFAULT
#define LS_MAX_KEY_DEFAULT (1024u * 1024u) /* 1 MiB */
#endif
#ifndef LS_MAX_VALUE_DEFAULT
#define LS_MAX_VALUE_DEFAULT (8u * 1024u * 1024u) /* 8 MiB */
#endif

// Struct definition and append_req_t now live in logstore_internal.h

// Time helper moved to worker unit: _ls_now_ns()

// Worker logic moved to logstore_worker.c

// Background fsync logic moved to logstore_worker.c

struct preload_ud {
    po_index_t *idx;
};
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
    PO_METRIC_COUNTER_INC("logstore.open.attempt");
    if (!cfg || !cfg->dir || !cfg->bucket)
        return NULL;
    char path[512];
    snprintf(path, sizeof(path), "%s/aof.log", cfg->dir);
    int fd = open(path, O_CREAT | O_RDWR, 0664); // need read for pread()
    if (fd < 0) {
        LOG_ERROR("logstore: open(%s) failed", path);
        PO_METRIC_COUNTER_INC("logstore.open.fail");
        return NULL;
    }

    db_env_t *env = NULL;
    if (db_env_open(cfg->dir, 8, cfg->map_size, &env) != 0) {
        LOG_ERROR("logstore: db_env_open failed");
        PO_METRIC_COUNTER_INC("logstore.open.fail");
        close(fd);
        return NULL;
    }
    db_bucket_t *idx = NULL;
    if (db_bucket_open(env, cfg->bucket, &idx) != 0) {
        LOG_ERROR("logstore: db_bucket_open(%s) failed", cfg->bucket);
        PO_METRIC_COUNTER_INC("logstore.open.fail");
        db_env_close(&env);
        close(fd);
        return NULL;
    }

    po_logstore_t *ls = calloc(1, sizeof(*ls));
    if (!ls) {
        db_bucket_close(&idx);
        db_env_close(&env);
        close(fd);
        PO_METRIC_COUNTER_INC("logstore.open.fail");
        return NULL;
    }
    ls->fd = fd;
    ls->env = env;
    ls->idx = idx;
    size_t cap = cfg->ring_capacity ? cfg->ring_capacity : 1024;
    ls->q = perf_ringbuf_create(cap);
    if (!ls->q) {
        po_logstore_close(&ls);
        PO_METRIC_COUNTER_INC("logstore.open.fail");
        return NULL;
    }
    ls->b = perf_batcher_create(ls->q, cfg->batch_size ? cfg->batch_size : 32);
    if (!ls->b) {
        po_logstore_close(&ls);
        PO_METRIC_COUNTER_INC("logstore.open.fail");
        return NULL;
    }
    ls->mem_idx = po_index_create(1024);
    if (!ls->mem_idx) {
        po_logstore_close(&ls);
        PO_METRIC_COUNTER_INC("logstore.open.fail");
        return NULL;
    }
    pthread_rwlock_init(&ls->idx_lock, NULL);
    ls->batch_size = cfg->batch_size ? cfg->batch_size : 32;
    ls->fsync_policy = cfg->fsync_policy;
    atomic_store(&ls->seq, 0);
    // preload in-memory index from LMDB to speed up reads
    struct preload_ud u = {.idx = ls->mem_idx};
    (void)db_iterate(ls->idx, preload_cb, &u);
    atomic_store(&ls->running, 1);
    if (ls->fsync_policy == PO_LS_FSYNC_INTERVAL && cfg->fsync_interval_ms > 0) {
        ls->fsync_interval_ns = (uint64_t)cfg->fsync_interval_ms * 1000000ull;
        ls->last_fsync_ns = _ls_now_ns();
    } else {
        ls->fsync_interval_ns = 0;
        ls->last_fsync_ns = 0;
    }
    if (ls->fsync_policy == PO_LS_FSYNC_EVERY_N) {
        ls->fsync_every_n = cfg->fsync_every_n ? cfg->fsync_every_n : 1;
        ls->batches_since_fsync = 0;
    } else {
        ls->fsync_every_n = 0;
        ls->batches_since_fsync = 0;
    }

    // Configure max key/value sizes (fallback to defaults if zero supplied)
    ls->max_key_bytes = cfg->max_key_bytes ? cfg->max_key_bytes : LS_MAX_KEY_DEFAULT;
    ls->max_value_bytes = cfg->max_value_bytes ? cfg->max_value_bytes : LS_MAX_VALUE_DEFAULT;
    // Enforce that configured values never exceed hard bounds to prevent wrap/overflow scenarios.
    if (ls->max_key_bytes > LS_HARD_KEY_MAX)
        ls->max_key_bytes = LS_HARD_KEY_MAX;
    if (ls->max_value_bytes > LS_HARD_VALUE_MAX)
        ls->max_value_bytes = LS_HARD_VALUE_MAX;

    // Optional rebuild on open moved to separate module
    (void)_ls_rebuild_on_open(ls, cfg);
    ls->nworkers = cfg->workers ? cfg->workers : 1;
    ls->workers = calloc(ls->nworkers, sizeof(pthread_t));
    if (!ls->workers) {
        po_logstore_close(&ls);
        return NULL;
    }
    for (unsigned i = 0; i < ls->nworkers; ++i) {
        if (pthread_create(&ls->workers[i], NULL, _ls_worker_main, ls) != 0) {
            ls->nworkers = i; // only join started
            po_logstore_close(&ls);
            PO_METRIC_COUNTER_INC("logstore.open.fail");
            return NULL;
        }
    }
    // Wait briefly for at least one worker to signal readiness to avoid early
    // appends racing before the worker enters its dequeue loop (which can
    // cause transient full ring observations under heavy startup burst).
    struct timespec _tw = {.tv_sec = 0, .tv_nsec = 5000000}; // 5ms sleep
    int spins = 0;
    while (!atomic_load(&ls->worker_ready) && spins < 100) { // up to ~500ms worst case
        nanosleep(&_tw, NULL);
        spins++;
    }
    if (cfg->background_fsync && ls->fsync_policy == PO_LS_FSYNC_INTERVAL &&
        ls->fsync_interval_ns) {
        ls->background_fsync = 1;
        atomic_store(&ls->fsync_thread_run, 1);
        if (pthread_create(&ls->fsync_thread, NULL, _ls_fsync_thread_main, ls) != 0) {
            ls->background_fsync = 0;
            atomic_store(&ls->fsync_thread_run, 0);
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
    if (!*pls)
        return;
    PO_METRIC_COUNTER_INC("logstore.close.calls");
    po_logstore_t *ls = *pls;
    atomic_store(&ls->running, 0);
    // enqueue sentinel to wake worker if blocked on eventfd
    append_req_t *sent = calloc(1, sizeof(*sent));
    if (sent) {
        ls->sentinel = sent;
        (void)perf_batcher_enqueue(ls->b, sent);
    }
    for (unsigned i = 0; i < ls->nworkers; ++i) {
        pthread_join(ls->workers[i], NULL);
    }
    // Additional defensive drain: free any leftover requests still in ring (should be rare)
    for (;;) {
        void *tmp = NULL;
        if (perf_ringbuf_dequeue(ls->q, &tmp) != 0)
            break;
        append_req_t *req = (append_req_t *)tmp;
        if (req && req != ls->sentinel) {
            // append_req_t uses a single allocation layout
            // [append_req_t][key bytes][value bytes]; only free top-level.
            free(req);
            atomic_fetch_sub(&ls->outstanding_reqs, 1);
        }
    }
    if (ls->background_fsync) {
        atomic_store(&ls->fsync_thread_run, 0);
        pthread_join(ls->fsync_thread, NULL);
    }
    if (ls->sentinel) {
        free(ls->sentinel);
        ls->sentinel = NULL;
    }
    if (ls->workers) {
        free(ls->workers);
        ls->workers = NULL;
    }
    if (ls->b)
        perf_batcher_destroy(&ls->b);
    if (ls->q)
        perf_ringbuf_destroy(&ls->q);
    if (ls->idx)
        db_bucket_close(&ls->idx);
    if (ls->env)
        db_env_close(&ls->env);
    if (ls->fd >= 0)
        close(ls->fd);
    if (ls->mem_idx)
        po_index_destroy(&ls->mem_idx);
    pthread_rwlock_destroy(&ls->idx_lock);
    size_t leaks = atomic_load(&ls->outstanding_reqs);
    if (leaks != 0) {
        LOG_ERROR("logstore: %zu outstanding append requests at close (freed defensively)", leaks);
        PO_METRIC_COUNTER_ADD("logstore.close.leaks", leaks);
    }
    free(ls);
    *pls = NULL;
}

int po_logstore_append(po_logstore_t *ls, const void *key, size_t keylen, const void *val,
                       size_t vallen) {
    if (!ls || !key ||
        _ls_validate_lengths(keylen, vallen, ls->max_key_bytes, ls->max_value_bytes) != 0) {
        PO_METRIC_COUNTER_INC("logstore.append.invalid");
        return -1;
    }
    // Reject appends after shutdown begins to avoid leaking requests that would
    // otherwise never be processed because workers have exited.
    if (!atomic_load(&ls->running)) {
        PO_METRIC_COUNTER_INC("logstore.append.after_close");
        return -1;
    }
    // Single allocation: [append_req_t][key bytes][val bytes]
    size_t total = sizeof(append_req_t) + keylen + vallen;
    append_req_t *r = malloc(total);
    if (!r)
        return -1;
    r->k = (uint8_t *)r + sizeof(append_req_t);
    r->v = (uint8_t *)r + sizeof(append_req_t) + keylen;
    r->klen = keylen;
    r->vlen = vallen;
    memcpy(r->k, key, keylen);
    memcpy(r->v, val, vallen);
    atomic_fetch_add(&ls->outstanding_reqs, 1);
    // Best-effort retry loop: the ring can become momentarily full during bursts.
    // Instead of failing immediately (causing spurious -1 to callers under load),
    // retry for a bounded period with short sleeps. This provides gentle
    // backpressure without busy spinning.
    // Adaptive retry strategy: keep retrying while running unless policy forbids overwrite.
    // We start with short sleeps and back off up to a cap to avoid excessive CPU usage under
    // sustained saturation, while strongly biasing toward eventual success instead of spurious -1.
    // Retry until success (or shutdown / policy override). The previous hard cap
    // introduced spurious failures under heavy concurrent append pressure which
    // caused tests to fail and leaked the in-flight allocation when the test
    // aborted early (skipping close). Removing the artificial cap makes the
    // API semantics "eventual success unless shutdown or overwrite-forbidden".
    // We retain an adaptive backoff to avoid busy spinning.
    long sleep_ns = 50 * 1000;                 // 50us initial
    const long sleep_ns_max = 2 * 1000 * 1000; // 2ms cap
    int attempt = 0;
    while (perf_batcher_enqueue(ls->b, r) < 0) {
        if (ls->never_overwrite || !atomic_load(&ls->running)) {
            atomic_fetch_sub(&ls->outstanding_reqs, 1);
            free(r);
            PO_METRIC_COUNTER_INC("logstore.append.enqueue_fail");
            return -1;
        }
        // Emit a metric periodically for observability of sustained contention.
        if ((attempt++ % 1000) == 0) {
            PO_METRIC_COUNTER_INC("logstore.append.retries");
        }
        struct timespec ts = {.tv_sec = 0, .tv_nsec = sleep_ns};
        nanosleep(&ts, NULL);
        if (sleep_ns < sleep_ns_max)
            sleep_ns = (sleep_ns * 3) / 2 + 10 * 1000; // mild growth with bias
    }
    PO_METRIC_COUNTER_INC("logstore.append.ok");
    PO_METRIC_COUNTER_ADD("logstore.append.bytes", (keylen + vallen));
    return 0;
}

int po_logstore_get(po_logstore_t *ls, const void *key, size_t keylen, void **out_val,
                    size_t *out_len) {
    if (!ls || !key || keylen == 0 || keylen > ls->max_key_bytes)
        return -1;
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
        if (rc != 0)
            return -1;
        if (tmplen != 12) {
            free(tmp);
            return -1;
        }
        memcpy(&off, tmp, 8);
        memcpy(&len, (uint8_t *)tmp + 8, 4);
        free(tmp);
        // optionally backfill mem index
        pthread_rwlock_wrlock(&ls->idx_lock);
        (void)po_index_put(ls->mem_idx, key, keylen, off, len);
        pthread_rwlock_unlock(&ls->idx_lock);
    }
    PO_METRIC_COUNTER_INC("logstore.get.hit_mem");
    // read from file
    uint32_t hdr[2];
    if (pread(ls->fd, hdr, sizeof(hdr), (off_t)off) != (ssize_t)sizeof(hdr))
        return -1;
    uint32_t kl = hdr[0];
    uint32_t vl = hdr[1];
    if (kl > 16 * 1024 * 1024 || vl != len)
        return -1;
    void *buf = malloc(vl);
    if (!buf)
        return -1;
    off_t val_off = (off_t)(off + sizeof(kl) + sizeof(vl) + (off_t)kl);
    if (pread(ls->fd, buf, vl, val_off) != (ssize_t)vl) {
        free(buf);
        return -1;
    }
    *out_val = buf;
    *out_len = vl;
    PO_METRIC_COUNTER_INC("logstore.get.ok");
    PO_METRIC_COUNTER_ADD("logstore.get.bytes", vl);
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
    memcpy(key, &ts_ns, 8);
    memcpy(key + 8, &seq, 8);
    size_t vlen = strnlen(line, LOGGER_MSG_MAX);
    (void)po_logstore_append(ls, key, sizeof key, line, vlen);
}

int po_logstore_attach_logger(po_logstore_t *ls) {
    // this requires logger to expose custom sink API; if unavailable, return -1
    extern int po_logger_add_sink_custom(void (*fn)(const char *, void *), void *ud);
    return po_logger_add_sink_custom(_ls_logger_sink, ls);
}

// Forward to modular implementation
int po_logstore_integrity_scan(po_logstore_t *ls, int prune_nonexistent,
                               po_logstore_integrity_stats *out_stats) {
    return _ls_integrity_scan(ls, prune_nonexistent, out_stats);
}

int po_logstore_debug_put_index(po_logstore_t *ls, const void *key, size_t keylen, uint64_t offset,
                                uint32_t len) {
    if (!ls || !key || keylen == 0)
        return -1;
    uint8_t iv[12];
    memcpy(iv, &offset, 8);
    memcpy(iv + 8, &len, 4);
    if (db_put(ls->idx, key, keylen, iv, sizeof iv) != 0)
        return -1;
    pthread_rwlock_wrlock(&ls->idx_lock);
    (void)po_index_put(ls->mem_idx, key, keylen, offset, len);
    pthread_rwlock_unlock(&ls->idx_lock);
    return 0;
}

int po_logstore_debug_lookup(po_logstore_t *ls, const void *key, size_t keylen, uint64_t *out_off,
                             uint32_t *out_len) {
    if (!ls || !key || keylen == 0 || keylen > ls->max_key_bytes || !out_off || !out_len)
        return -1;
    uint64_t off = 0;
    uint32_t len = 0;
    pthread_rwlock_rdlock(&ls->idx_lock);
    int have = po_index_get(ls->mem_idx, key, keylen, &off, &len);
    pthread_rwlock_unlock(&ls->idx_lock);
    if (have != 0) {
        void *tmp = NULL;
        size_t tmplen = 0;
        if (db_get(ls->idx, key, keylen, &tmp, &tmplen) != 0)
            return -1;
        if (tmplen != 12) {
            free(tmp);
            return -1;
        }
        memcpy(&off, tmp, 8);
        memcpy(&len, (uint8_t *)tmp + 8, 4);
        free(tmp);
    }
    *out_off = off;
    *out_len = len;
    return 0;
}
