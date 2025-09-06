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
} po_logstore_fsync_policy_t;

typedef struct po_logstore_cfg {
	const char *dir;           // base directory for data files
	const char *bucket;        // LMDB bucket name for index
	size_t map_size;           // LMDB map size in bytes
	size_t ring_capacity;      // queue capacity (power of two recommended)
	size_t batch_size;         // max records per flush batch (e.g., 32)
	po_logstore_fsync_policy_t fsync_policy; // durability policy
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

#ifdef __cplusplus
}
#endif

#endif // POSTOFFICE_LOGSTORE_H
