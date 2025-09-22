/**
 * @file debounce.c
 * @ingroup tui_util
 * @brief HOW: Debounce utility implementation.
 */

#include "debounce.h"

#include <stdlib.h>

struct debounce {
    double interval_ms;
    double scheduled_at;
    debounce_cb cb;
    void *ud;
    int active;
};

debounce *debounce_create(double interval_ms, debounce_cb cb, void *ud) {
    debounce *d = (debounce *)calloc(1, sizeof(*d));
    if (!d)
        return NULL;
    d->interval_ms = interval_ms;
    d->cb = cb;
    d->ud = ud;
    return d;
}
void debounce_destroy(debounce *d) {
    free(d);
}

void debounce_bump(debounce *d) {
    if (!d)
        return;
    d->scheduled_at = -1.0;
    d->active = 1;
}
bool debounce_tick(debounce *d, double now_ms) {
    if (!d || !d->active)
        return false;
    if (d->scheduled_at < 0.0) {
        d->scheduled_at = now_ms + d->interval_ms;
        return false;
    }
    if (now_ms >= d->scheduled_at) {
        d->active = 0;
        if (d->cb)
            d->cb(d->ud);
        return true;
    }
    return false;
}
/**
 * @file debounce.c
 * @ingroup tui_util
 * @brief HOW: Debounce helper for noisy inputs (e.g., resize).
 */

// Implementation will be added in subsequent milestones.
