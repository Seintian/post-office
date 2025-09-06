/**
 * @file logstore.h
 * @brief Append-only log store with async batching and LMDB index.
 */

#ifndef POSTOFFICE_LOGSTORE_H
#define POSTOFFICE_LOGSTORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct po_logstore po_logstore_t;

// Fsync policy for durability vs throughput
typedef enum po_logstore_fsync_policy {
	PO_LS_FSYNC_NONE = 0,     // never fsync (fastest)
	PO_LS_FSYNC_EACH_BATCH = 1, // fsync() after each drained batch
	PO_LS_FSYNC_INTERVAL = 2,  // fsync() at most every fsync_interval_ms
	PO_LS_FSYNC_EVERY_N = 3,   // fsync() after every N drained batches (fsync_every_n)
} po_logstore_fsync_policy_t;

typedef struct po_logstore_cfg {
	const char *dir;           // base directory for data files
	const char *bucket;        // LMDB bucket name for index
	size_t map_size;           // LMDB map size in bytes
	size_t ring_capacity;      // queue capacity (power of two recommended)
	size_t batch_size;         // max records per flush batch (e.g., 32)
	po_logstore_fsync_policy_t fsync_policy; // durability policy
	unsigned fsync_interval_ms; // used when policy == PO_LS_FSYNC_INTERVAL
	unsigned fsync_every_n;    // used when policy == PO_LS_FSYNC_EVERY_N (0 => treat as 1)
	int rebuild_on_open;       // if non-zero: rebuild index from log file on open
	int truncate_on_rebuild;   // if non-zero: truncate corrupted tail after rebuild scan
	int background_fsync;      // if non-zero and policy==INTERVAL: perform fsync in bg thread
	size_t max_key_bytes;      // maximum accepted key length (0 => default)
	size_t max_value_bytes;    // maximum accepted value length (0 => default)
} po_logstore_cfg;

// Open with explicit config
po_logstore_t *po_logstore_open_cfg(const po_logstore_cfg *cfg);

// Convenience open with defaults
po_logstore_t *po_logstore_open(const char *dir, const char *bucket, size_t map_size,
								size_t ring_capacity);
void po_logstore_close(po_logstore_t **ls);

int po_logstore_append(po_logstore_t *ls, const void *key, size_t keylen,
					   const void *val, size_t vallen);

int po_logstore_get(po_logstore_t *ls, const void *key, size_t keylen,
					void **out_val, size_t *out_len);

// Optional integration: append formatted logger lines into the logstore
// If available, this registers a custom sink in the logger that writes
// each formatted log line into the logstore (keyed by time+seq).
int po_logstore_attach_logger(po_logstore_t *ls);

// Integrity scan statistics
typedef struct po_logstore_integrity_stats {
	size_t scanned; // LMDB entries scanned (for scan) OR file records parsed (for rebuild)
	size_t valid;   // valid entries confirmed
	size_t pruned;  // stale/invalid entries deleted (only if prune enabled)
	size_t errors;  // parsing/IO errors encountered (file truncations etc.)
} po_logstore_integrity_stats;

// Perform an integrity scan over the LMDB index validating that each (offset,len)
// points to a well-formed record fully contained in the append-only file. If
// prune_nonexistent is non-zero, stale entries are deleted from LMDB and removed
// from the in-memory index. Returns 0 on success (even if some entries were
// pruned) and -1 on unrecoverable IO error.
int po_logstore_integrity_scan(po_logstore_t *ls, int prune_nonexistent,
							   po_logstore_integrity_stats *out_stats);

// Debug/testing helper: insert a raw index entry (offset,len) into backing LMDB without
// writing anything to the append file. Used to create stale entries for integrity tests.
// Returns 0 on success, -1 on error. Not intended for production use.
int po_logstore_debug_put_index(po_logstore_t *ls, const void *key, size_t keylen,
								uint64_t offset, uint32_t len);

// Debug: look up raw offset/length for a key via LMDB/memory index. Returns 0 on success.
int po_logstore_debug_lookup(po_logstore_t *ls, const void *key, size_t keylen,
							 uint64_t *out_off, uint32_t *out_len);

#ifdef __cplusplus
}
#endif

#endif // POSTOFFICE_LOGSTORE_H
