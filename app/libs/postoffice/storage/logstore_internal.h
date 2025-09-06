/**
 * @file internal/logstore_internal.h
 * @brief Internal shared structures and helpers for logstore modularized implementation.
 *
 * Not part of the public installed interface. Only translation units within
 * the storage module should include this header.
 */
#ifndef POSTOFFICE_LOGSTORE_INTERNAL_H
#define POSTOFFICE_LOGSTORE_INTERNAL_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/types.h>

#include "storage/logstore.h"
#include "perf/batcher.h"
#include "perf/ringbuf.h"
#include "storage/db_lmdb.h"
#include "storage/index.h"

// Hard upper bounds (guard against corruption / unreasonable values)
#define LS_HARD_KEY_MAX   (32u * 1024u * 1024u)  /* 32 MiB */
#define LS_HARD_VALUE_MAX (128u * 1024u * 1024u) /* 128 MiB */

// Internal append request
typedef struct {
    void *k;
    size_t klen;
    void *v;
    size_t vlen;
} append_req_t;

struct po_logstore {
    int fd;                                  // append-only file descriptor
    db_env_t *env;                           // LMDB environment
    db_bucket_t *idx;                        // LMDB bucket for index
    po_perf_ringbuf_t *q;                       // submission queue
    po_perf_batcher_t *b;                       // batching helper
    pthread_t worker;                        // flush worker thread
    _Atomic int running;                     // running flag
    po_index_t *mem_idx;                     // fast path in-memory index
    pthread_rwlock_t idx_lock;               // RW lock protecting mem_idx
    size_t batch_size;                       // configured batch size
    po_logstore_fsync_policy_t fsync_policy; // durability policy
    _Atomic uint64_t seq;                    // sequence for logger sink keys
    append_req_t *sentinel;                  // sentinel request for shutdown
    uint64_t fsync_interval_ns;              // interval (ns) for interval policy
    uint64_t last_fsync_ns;                  // last fsync timestamp
    unsigned fsync_every_n;                  // threshold for EVERY_N policy
    unsigned batches_since_fsync;            // counter for EVERY_N policy
    int background_fsync;                    // background fsync thread enabled
    pthread_t fsync_thread;                  // background fsync thread
    _Atomic int fsync_thread_run;            // background fsync running flag
    size_t max_key_bytes;                    // configured max key size
    size_t max_value_bytes;                  // configured max value size
    _Atomic size_t outstanding_reqs;         // diagnostic: live append requests (debug/leak guard)
};

// Length validation helper (0 ok, -1 invalid)
static inline int _ls_validate_lengths(size_t klen, size_t vlen, size_t maxk, size_t maxv) {
    if (klen == 0) return -1;
    if (klen > maxk) return -1;
    if (vlen > maxv) return -1;
    if (klen > LS_HARD_KEY_MAX) return -1;
    if (vlen > LS_HARD_VALUE_MAX) return -1;
    return 0;
}

uint64_t _ls_now_ns(void);                      // time helper
void * _ls_worker_main(void *arg);               // worker thread
void * _ls_fsync_thread_main(void *arg);         // fsync thread
int _ls_rebuild_on_open(po_logstore_t *ls, const po_logstore_cfg *cfg); // rebuild
int _ls_integrity_scan(po_logstore_t *ls, int prune_nonexistent,
                       po_logstore_integrity_stats *out_stats);         // integrity scan

#endif /* POSTOFFICE_LOGSTORE_INTERNAL_H */
