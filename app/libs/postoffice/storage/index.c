/**
 * @file index.c
 */

#include "storage/index.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable/hashtable.h"

typedef struct {
    void *key;
    size_t keylen;
    uint64_t offset;
    uint32_t len;
} idx_entry_t;

struct po_index {
    po_hashtable_t *ht;
};

static int cmp_bin(const void *a, const void *b) {
    const idx_entry_t *ea = (const idx_entry_t *)a;
    const idx_entry_t *eb = (const idx_entry_t *)b;
    if (ea->keylen != eb->keylen)
        return 1;
    return ea->key && eb->key ? memcmp(ea->key, eb->key, ea->keylen) : 1;
}

static unsigned long hash_bin(const void *a) {
    const idx_entry_t *e = (const idx_entry_t *)a;
    const unsigned char *p = (const unsigned char *)e->key;
    unsigned long h = 1469598103934665603UL; // FNV-1a 64
    for (size_t i = 0; i < e->keylen; ++i) {
        h ^= p[i];
        h *= 1099511628211UL;
    }
    return (unsigned long)h;
}

po_index_t *po_index_create(size_t expected_entries) {
    po_index_t *idx = (po_index_t *)calloc(1, sizeof(*idx));
    if (!idx)
        return NULL;
    idx->ht = po_hashtable_create_sized(cmp_bin, hash_bin, expected_entries ? expected_entries : 128);
    if (!idx->ht) {
        free(idx);
        return NULL;
    }
    return idx;
}

void po_index_destroy(po_index_t **pidx) {
    if (!*pidx)
        return;
    po_index_t *idx = *pidx;
    po_hashtable_iter_t *it = po_hashtable_iterator(idx->ht);
    while (it && po_hashtable_iter_next(it)) {
        idx_entry_t *e = po_hashtable_iter_key(it);
        free(e->key);
        free(e);
    }
    free(it);
    po_hashtable_destroy(&idx->ht);
    free(idx);
    *pidx = NULL;
}

int po_index_put(po_index_t *idx, const void *key, size_t keylen, uint64_t offset, uint32_t len) {
    if (!idx || !key || keylen == 0)
        return -1;
    idx_entry_t lookup = {.key = (void *)key, .keylen = keylen};
    idx_entry_t *existing = po_hashtable_get(idx->ht, &lookup);
    if (existing) {
        existing->offset = offset;
        existing->len = len;
        return 0;
    }
    idx_entry_t *e = (idx_entry_t *)calloc(1, sizeof(*e));
    if (!e)
        return -1;
    e->key = malloc(keylen);
    if (!e->key) {
        free(e);
        return -1;
    }
    memcpy(e->key, key, keylen);
    e->keylen = keylen;
    e->offset = offset;
    e->len = len;
    int ins = po_hashtable_put(idx->ht, e, e);
    if (ins < 0) { // allocation or resize failure
        free(e->key);
        free(e);
        return -1;
    }
    // ins==1 new, ins==0 replaced (shouldn't happen given uniqueness) both success
    return 0;
}

int po_index_get(const po_index_t *idx, const void *key, size_t keylen, uint64_t *out_offset,
                 uint32_t *out_len) {
    if (!idx || !key || keylen == 0)
        return -1;
    idx_entry_t lookup = {.key = (void *)key, .keylen = keylen};
    idx_entry_t *e = po_hashtable_get(idx->ht, &lookup);
    if (!e)
        return -1;
    if (out_offset)
        *out_offset = e->offset;
    if (out_len)
        *out_len = e->len;
    return 0;
}

int po_index_remove(po_index_t *idx, const void *key, size_t keylen) {
    if (!idx || !key || keylen == 0)
        return -1;
    idx_entry_t lookup = {.key = (void *)key, .keylen = keylen};
    idx_entry_t *e = po_hashtable_get(idx->ht, &lookup);
    if (!e)
        return -1;
    po_hashtable_remove(idx->ht, e);
    free(e->key);
    free(e);
    return 0;
}
