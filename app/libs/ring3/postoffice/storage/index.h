/**
 * @file index.h
 * @ingroup logstore
 * @brief In-memory key→(offset,length) index supporting append-only log lookups.
 *
 * Purpose
 * -------
 * Maintains only the latest mapping for a binary key to `(file_offset,value_length)`
 * inside the append-only log data file. Older versions for a key are implicitly
 * shadowed; no historical retention or MVCC semantics are provided.
 *
 * Characteristics
 * --------------
 *  - In-memory only (rebuilt from log scan if needed at startup).
 *  - O(1) expected operations using an internal hash table (open addressed).
 *  - Keys/values (offset,len) pairs stored by value; key bytes are copied so
 *    caller buffers may be transient.
 *  - Not thread-safe; external synchronization required if accessed from
 *    multiple threads concurrently (logstore coordinates access internally).
 *
 * Error Handling
 * --------------
 *  - put returns 0 on success, -1 on allocation failure (errno=ENOMEM).
 *  - get returns 0 on success, -1 if key absent (errno=ENOENT).
 *  - remove returns 0 on success, -1 if key absent (errno=ENOENT).
 *
 * @see logstore.h For higher-level append/get operations that delegate here.
 */

#ifndef POSTOFFICE_STORAGE_INDEX_H
#define POSTOFFICE_STORAGE_INDEX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct po_index po_index_t;

po_index_t *po_index_create(size_t expected_entries);
void po_index_destroy(po_index_t **idx);

int po_index_put(po_index_t *idx, const void *key, size_t keylen, uint64_t offset, uint32_t len);
int po_index_get(const po_index_t *idx, const void *key, size_t keylen, uint64_t *out_offset,
                 uint32_t *out_len);
int po_index_remove(po_index_t *idx, const void *key, size_t keylen);

#ifdef __cplusplus
}
#endif

#endif // POSTOFFICE_STORAGE_INDEX_H
