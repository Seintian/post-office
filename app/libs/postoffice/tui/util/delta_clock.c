/**
 * @file delta_clock.c
 * @ingroup tui_util
 * @brief HOW: Frame-to-frame delta time computation.
 */

#define _POSIX_C_SOURCE 200809L
#include "delta_clock.h"

#include <stdlib.h>
#include <time.h>

struct delta_clock {
    struct timespec last;
    int initialized;
};

static inline double ts_to_ms(const struct timespec *ts) {
    return (double)ts->tv_sec * 1000.0 + (double)ts->tv_nsec / 1e6;
}

delta_clock *delta_clock_create(void) {
    return (delta_clock *)calloc(1, sizeof(delta_clock));
}
void delta_clock_destroy(delta_clock *dc) {
    free(dc);
}

double delta_clock_tick(delta_clock *dc) {
    if (!dc)
        return 0.0;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!dc->initialized) {
        dc->last = now;
        dc->initialized = 1;
        return 0.0;
    }
    double d = ts_to_ms(&now) - ts_to_ms(&dc->last);
    dc->last = now;
    return d;
}
