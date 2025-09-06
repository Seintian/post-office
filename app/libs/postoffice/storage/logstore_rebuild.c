/**
 * @file logstore_rebuild.c
 * @brief Rebuild-on-open logic extracted from original monolithic file.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "log/logger.h"
#include "storage/logstore_internal.h"

int _ls_rebuild_on_open(po_logstore_t *ls, const po_logstore_cfg *cfg) {
    if (!cfg->rebuild_on_open)
        return 0;
    off_t cursor = 0;
    off_t last_good_end = 0;
    for (;;) {
        uint32_t kl = 0, vl = 0;
        ssize_t rd = pread(ls->fd, &kl, sizeof(kl), cursor);
        if (rd == 0) {
            last_good_end = cursor;
            break;
        }
        if (rd != (ssize_t)sizeof(kl))
            break;
        rd = pread(ls->fd, &vl, sizeof(vl), cursor + (off_t)sizeof(kl));
        if (rd != (ssize_t)sizeof(vl))
            break;
        if (_ls_validate_lengths(kl, vl, ls->max_key_bytes, ls->max_value_bytes) != 0)
            break;
        size_t rec_sz = sizeof(uint32_t) * 2 + (size_t)kl + (size_t)vl;
        void *kbuf = malloc(kl);
        if (!kbuf)
            break;
        rd = pread(ls->fd, kbuf, kl, cursor + (off_t)sizeof(uint32_t) * 2);
        if (rd != (ssize_t)kl) {
            free(kbuf);
            break;
        }
        if (vl > 0) {
            char probe;
            rd = pread(ls->fd, &probe, 1, cursor + (off_t)sizeof(uint32_t) * 2 + kl + vl - 1);
            if (rd != 1) {
                free(kbuf);
                break;
            }
        }
        uint8_t iv[12];
        memcpy(iv, &cursor, 8);
        memcpy(iv + 8, &vl, 4);
        (void)db_put(ls->idx, kbuf, kl, iv, sizeof iv);
        pthread_rwlock_wrlock(&ls->idx_lock);
        (void)po_index_put(ls->mem_idx, kbuf, kl, (uint64_t)cursor, vl);
        pthread_rwlock_unlock(&ls->idx_lock);
        free(kbuf);
        cursor += (off_t)rec_sz;
        last_good_end = cursor;
    }
    if (cfg->truncate_on_rebuild) {
        if (ftruncate(ls->fd, last_good_end) != 0) {
            LOG_WARN("logstore: ftruncate(%lld) failed during rebuild", (long long)last_good_end);
        }
    }
    return 0;
}
