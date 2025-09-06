/**
 * @file logstore_integrity.c
 * @brief Integrity scan implementation split from original file.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "storage/logstore_internal.h"

struct scan_ud {
    po_logstore_t *ls;
    int prune;
    po_logstore_integrity_stats *st;
    off_t end;
};

static int _integrity_cb(const void *k, size_t klen, const void *v, size_t vlen, void *pud) {
    struct scan_ud *s = (struct scan_ud *)pud;
    (void)klen;
    uint64_t off;
    uint32_t len;
    uint32_t kl_real;
    uint32_t vl_real;
    s->st->scanned++;
    if (vlen != 12) {
        s->st->errors++;
        return 0;
    }
    memcpy(&off, v, 8);
    memcpy(&len, (const uint8_t *)v + 8, 4);
    if (off + sizeof(uint32_t) * 2 > (uint64_t)s->end) {
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
        pread(s->ls->fd, &vl_real, sizeof(vl_real), (off_t)(off + sizeof(uint32_t))) !=
            (ssize_t)sizeof(vl_real)) {
        s->st->errors++;
        return 0;
    }
    if (off + sizeof(uint32_t) * 2 + kl_real + vl_real > (uint64_t)s->end) {
        if (s->prune) {
            (void)db_delete(s->ls->idx, k, klen);
            pthread_rwlock_wrlock(&s->ls->idx_lock);
            (void)po_index_remove(s->ls->mem_idx, k, klen);
            pthread_rwlock_unlock(&s->ls->idx_lock);
            s->st->pruned++;
        }
        return 0;
    }
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
    if (!kbuf) {
        s->st->errors++;
        return 0;
    }
    if (pread(s->ls->fd, kbuf, kl_real, (off_t)(off + sizeof(uint32_t) * 2)) != (ssize_t)kl_real) {
        free(kbuf);
        s->st->errors++;
        return 0;
    }
    int key_match = (memcmp(kbuf, k, kl_real) == 0);
    free(kbuf);
    if (!key_match || vl_real != len) {
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

int _ls_integrity_scan(po_logstore_t *ls, int prune_nonexistent,
                       po_logstore_integrity_stats *out_stats) {
    if (!ls)
        return -1;

    po_logstore_integrity_stats stats = {0};
    off_t end = lseek(ls->fd, 0, SEEK_END);
    if (end < 0)
        return -1;

    struct scan_ud ud = {ls, prune_nonexistent, &stats, end};
    (void)db_iterate(ls->idx, _integrity_cb, &ud);

    if (out_stats)
        *out_stats = stats;
    return 0;
}
