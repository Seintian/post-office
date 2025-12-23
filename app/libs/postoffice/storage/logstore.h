/**
 * @file logstore.h
 * @ingroup logstore
 * @brief Append-only log store with asynchronous batching and LMDB-backed
 *        key->(offset,length) index.
 *
 * Overview
 * ========
 * The log store consists of:
 *  - A preallocated / append-only data file (or sequence) storing variable
 *    length records: [key_len][val_len][key bytes][value bytes].
 *  - An LMDB database mapping key -> (file_offset, value_length) enabling
 *    O(log n) (LMDB btree) lookups independent of append batching.
 *  - An in-memory batching queue (perf ring buffer) + background flush worker
 *    that coalesces multiple append requests to reduce syscall + fsync
 *    frequency (policy-dependent).
 *
 * Concurrency Model
 * -----------------
 * Appends are enqueued quickly (copying key/value into an intermediate frame
 * or referencing buffers) and return after being accepted into the ring
 * buffer; a background thread drains the queue, writes records sequentially
 * and updates LMDB within a single batch transaction. Gets perform a key
 * lookup in LMDB followed by an on-demand read from the data file.
 *
 * Durability Policies (see po_logstore_fsync_policy_t)
 * -----------------------------------------------------
 *  - NONE: never fsync – highest throughput, risk of data loss on crash.
 *  - EACH_BATCH: fsync after every successful flush of queued records.
 *  - INTERVAL: fsync at most once per configured time interval.
 *  - EVERY_N: fsync after every N flush batches.
 *
 * Rebuild / Integrity
 * -------------------
 * If `rebuild_on_open` is set in the config, the store scans the entire data
 * file reconstructing index entries (optionally truncating a corrupt tail when
 * `truncate_on_rebuild` is non-zero). This enables crash recovery when the
 * index is stale or missing.
 *
 * Error Handling
 * --------------
 * All functions return 0 on success and -1 on failure unless otherwise noted,
 * setting errno to an LMDB, I/O, or validation error (EINVAL, ENOMEM, EIO,
 * MDB_MAP_FULL, etc.). Debug helpers follow the same convention.
 *
 * @see storage.h Umbrella lifecycle / default instance management.
 * @see po_logstore_fsync_policy_t Durability policy enum (defined below).
 * @see po_storage_logstore Retrieve default instance after ::po_storage_init().
 */

#ifndef POSTOFFICE_LOGSTORE_H
#define POSTOFFICE_LOGSTORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct po_logstore po_logstore_t; //!< Opaque log store handle

/**
 * @brief Fsync policy controlling durability vs throughput trade-off.
 *
 * See file-level documentation for higher level discussion.
 */
typedef enum po_logstore_fsync_policy {
    PO_LS_FSYNC_NONE = 0,       //!< Never fsync (fastest; crash may lose latest batches)
    PO_LS_FSYNC_EACH_BATCH = 1, //!< fsync() after each drained batch (lower latency for durability)
    PO_LS_FSYNC_INTERVAL = 2,   //!< fsync() at most once per fsync_interval_ms window
    PO_LS_FSYNC_EVERY_N = 3,    //!< fsync() after every N drained batches (see fsync_every_n)
} po_logstore_fsync_policy_t;

/**
 * @brief Configuration for ::po_logstore_open_cfg().
 *
 * Fields with value 0 (or NULL) may map to internal defaults where noted.
 */
typedef struct po_logstore_cfg {
    const char *dir;                         //!< Base directory for data / index files.
    const char *bucket;                      //!< LMDB database (bucket) name.
    size_t map_size;                         //!< LMDB map size (0 => library default heuristic).
    size_t ring_capacity;                    //!< Batching queue capacity (power-of-two recommended).
    size_t batch_size;                       //!< Max records per flush batch (>=1; influences latency).
    po_logstore_fsync_policy_t fsync_policy; //!< Durability policy.
    unsigned fsync_interval_ms;              //!< Interval (ms) when policy == PO_LS_FSYNC_INTERVAL.
    unsigned fsync_every_n;                  //!< N for policy == PO_LS_FSYNC_EVERY_N (0 => treated as 1).
    int rebuild_on_open;                     //!< Non-zero: scan data file to rebuild index.
    int truncate_on_rebuild;                 //!< Non-zero: truncate corrupt tail discovered during rebuild.
    int background_fsync;                    //!< Non-zero: perform interval fsync in background thread.
    size_t max_key_bytes;                    //!< Max allowed key length (0 => internal default / limit).
    size_t max_value_bytes;                  //!< Max allowed value length (0 => internal default / limit).
    unsigned workers;                        //!< Parallel flush workers (0 => 1). Experimental.
} po_logstore_cfg;

/**
 * @brief Open a log store with explicit configuration.
 *
 * Performs directory setup (creating if absent), initializes LMDB environment
 * and spawns background flush machinery. Returns NULL on error (errno set) and
 * does not leak partial resources.
 */
/**
 * @brief Open a log store with explicit configuration.
 *
 * Performs directory setup (creating if absent), initializes LMDB environment
 * and spawns background flush machinery. Returns NULL on error (errno set) and
 * does not leak partial resources.
 * @param[in] cfg Configuration structure.
 * @return Handle to opened logstore or NULL.
 * @note Thread-safe: No (Must be unique/exclusive per call).
 */
po_logstore_t *po_logstore_open_cfg(const po_logstore_cfg *cfg);

/**
 * @brief Convenience open using a subset of configuration parameters.
 *
 * Provides sane default batching / fsync settings suitable for development or
 * low-throughput environments. For performance tuning use
 * ::po_logstore_open_cfg().
 * @param[in] dir Base directory.
 * @param[in] bucket Bucket name.
 * @param[in] map_size LMDB map size.
 * @param[in] ring_capacity Ring buffer capacity.
 * @return Handle or NULL.
 * @note Thread-safe: No.
 */
po_logstore_t *po_logstore_open(const char *dir, const char *bucket, size_t map_size,
                                size_t ring_capacity);
void po_logstore_close(po_logstore_t **ls);

/**
 * @brief Append (key,value) pair to the log store.
 *
 * Non-blocking with respect to durability: returns after enqueue or immediate
 * validation failure. Actual file write + LMDB index update occurs in a flush
 * cycle. Keys must not exceed `max_key_bytes` (if non-zero). Values must not
 * exceed `max_value_bytes` (if non-zero).
 *
 * @param[in] ls Store handle.
 * @param[in] key Key data.
 * @param[in] keylen Key length.
 * @param[in] val Value data.
 * @param[in] vallen Value length.
 * @return 0 on success (enqueued), -1 on validation, allocation, or queue full
 *         error (errno set: EINVAL size constraints, ENOSPC internal queue full,
 *         ENOMEM allocation, LMDB codes for environment issues, etc.).
 * @note Thread-safe: Yes.
 */
int po_logstore_append(po_logstore_t *ls, const void *key, size_t keylen, const void *val,
                       size_t vallen);

/**
 * @brief Retrieve value for a key (point-in-time consistent with flushed state).
 *
 * Pending (not yet flushed) appends may not be visible to ::po_logstore_get().
 * Caller takes ownership of the returned value buffer (allocated internally);
 * an explicit free function may be provided in implementation (not shown here)
 * or the buffer could be malloc-backed—consult implementation for lifetime.
 *
 * @param[in] ls      Store handle.
 * @param[in] key     Key bytes.
 * @param[in] keylen  Key length in bytes.
 * @param[out] out_val Output pointer to allocated value buffer (set on success).
 * @param[out] out_len Output length of value in bytes.
 * @return 0 on success (value found), -1 if not found or on error (errno = ENOENT
 *         for missing, or LMDB / I/O error code).
 * @note Thread-safe: Yes.
 */
int po_logstore_get(po_logstore_t *ls, const void *key, size_t keylen, void **out_val,
                    size_t *out_len);

/**
 * @brief Attach a logger sink that appends each formatted log line.
 *
 * Log line keys may encode a time + sequence pair ensuring uniqueness. The
 * sink is idempotent if attached more than once (duplicate suppress logic
 * resides in the logger subsystem or here depending on implementation).
 * @param[in] ls Store handle.
 * @return 0 on success, -1 on failure.
 * @note Thread-safe: Yes.
 */
int po_logstore_attach_logger(po_logstore_t *ls);

/**
 * @brief Statistics describing integrity scan or rebuild outcomes.
 */
typedef struct po_logstore_integrity_stats {
    size_t scanned; //!< Entries or file records inspected.
    size_t valid;   //!< Entries validated as consistent.
    size_t pruned;  //!< Stale / invalid entries removed when pruning enabled.
    size_t errors;  //!< Parsing or I/O errors (truncations, corruption, etc.).
} po_logstore_integrity_stats;

/**
 * @brief Validate LMDB index entries against on-disk data file structure.
 *
 * Each key's referenced (offset,len) pair is checked to ensure it lies within
 * the file and corresponds to a syntactically valid record header. When
 * @p prune_nonexistent is non-zero, stale index entries referencing missing or
 * corrupt records are removed. Scanning continues in the presence of corrupt
 * entries unless a fatal I/O error occurs.
 *
 * @param[in] ls               Store handle.
 * @param[in] prune_nonexistent Non-zero to delete stale entries.
 * @param[out] out_stats        Optional stats output (may be NULL).
 * @return 0 on success (even if pruning removed entries); -1 on unrecoverable
 *         I/O error (errno set).
 * @note Thread-safe: No (Recommended exclusive use).
 */
int po_logstore_integrity_scan(po_logstore_t *ls, int prune_nonexistent,
                               po_logstore_integrity_stats *out_stats);

/**
 * @brief DEBUG: Insert raw (offset,len) index entry without writing to log.
 *
 * Used exclusively for test harnesses to simulate stale or orphaned index
 * entries ahead of an integrity scan. Not for production use.
 * @return 0 on success, -1 on error.
 */
int po_logstore_debug_put_index(po_logstore_t *ls, const void *key, size_t keylen, uint64_t offset,
                                uint32_t len);

/**
 * @brief DEBUG: Lookup raw (offset,len) pair for a key.
 * @return 0 on success (values written), -1 if missing (errno=ENOENT) or error.
 */
int po_logstore_debug_lookup(po_logstore_t *ls, const void *key, size_t keylen, uint64_t *out_off,
                             uint32_t *out_len);

#ifdef __cplusplus
}
#endif

#endif // POSTOFFICE_LOGSTORE_H
