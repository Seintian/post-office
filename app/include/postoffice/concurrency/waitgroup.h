#ifndef PO_CONCURRENCY_WAITGROUP_H
#define PO_CONCURRENCY_WAITGROUP_H

#include <stdint.h>

typedef struct waitgroup_s waitgroup_t;

waitgroup_t *wg_create(void);
void wg_add(waitgroup_t *wg, int count);
void wg_done(waitgroup_t *wg);
void wg_wait(waitgroup_t *wg);
int wg_active_count(waitgroup_t *wg);
void wg_destroy(waitgroup_t *wg);

#endif
