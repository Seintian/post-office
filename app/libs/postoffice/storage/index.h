/**
 * @file index.h
 * @brief In-memory key→(offset,length) index for the log store.
 */

#ifndef POSTOFFICE_STORAGE_INDEX_H
#define POSTOFFICE_STORAGE_INDEX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct po_index po_index_t;

po_index_t *po_index_create(size_t expected_entries);
void po_index_destroy(po_index_t **idx);

int po_index_put(po_index_t *idx, const void *key, size_t keylen, uint64_t offset, uint32_t len);
int po_index_get(const po_index_t *idx, const void *key, size_t keylen,
				 uint64_t *out_offset, uint32_t *out_len);
int po_index_remove(po_index_t *idx, const void *key, size_t keylen);

#ifdef __cplusplus
}
#endif

#endif // POSTOFFICE_STORAGE_INDEX_H
