#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "perf/perf.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "hashtable/hashtable.h"
#include "perf/batcher.h"
#include "perf/ringbuf.h"
#include "perf/zerocopy.h"
#include "utils/errors.h"

// -----------------------------------------------------------------------------
// Internal Event Definitions
// -----------------------------------------------------------------------------
typedef enum {
    EV_COUNTER_ADD,
    EV_TIMER_START,
    EV_TIMER_STOP,
    EV_HISTOGRAM_RECORD,
    EV_SHUTDOWN
} perf_ev_type_t;

typedef struct {
    perf_ev_type_t type;
    const char *name;
    uint64_t arg; // delta for counters, value for histogram
} perf_event_t;

// -----------------------------------------------------------------------------
// Internal Structures & Global Context
// -----------------------------------------------------------------------------
struct po_perf_counter {
    atomic_uint_fast64_t value;
};
struct po_perf_timer {
    struct timespec start;
    atomic_uint_fast64_t total_ns;
};
struct po_perf_histogram {
    atomic_uint_fast64_t *bins;
    atomic_uint_fast64_t *counts;
    size_t nbins;
};

static struct {
    po_hashtable_t *counters;
    po_hashtable_t *timers;
    po_hashtable_t *histograms;
    int initialized;

    // async infrastructure
    po_perf_ringbuf_t *event_q;
    po_perf_batcher_t *ev_batcher;
    perf_zcpool_t *fmt_pool;
    pthread_t worker_tid;
} ctx = {0};

// -----------------------------------------------------------------------------
// Utility: hash & compare strings
// -----------------------------------------------------------------------------
static unsigned long hash_string(const void *k) {
    const unsigned char *s = k;
    unsigned long h = 5381;
    for (unsigned char c; (c = *s++);)
        h = ((h << 5) + h) + c;
    return h;
}
static int compare_strings(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

// -----------------------------------------------------------------------------
// Worker Thread: drain events and dispatch into hash tables
// -----------------------------------------------------------------------------
static void dispatch_event(const perf_event_t *e) {
    switch (e->type) {
    case EV_COUNTER_ADD: {
        struct po_perf_counter *ctr = po_hashtable_get(ctx.counters, e->name);
        if (!ctr) {
            ctr = calloc(1, sizeof(*ctr));
            atomic_init(&ctr->value, 0);
            po_hashtable_put(ctx.counters, strdup(e->name), ctr);
        }

        atomic_fetch_add(&ctr->value, e->arg);
        break;
    }
    case EV_TIMER_START: {
        struct po_perf_timer *tmr = NULL;
        if (e->name)
            tmr = po_hashtable_get(ctx.timers, e->name);

        if (tmr)
            clock_gettime(CLOCK_MONOTONIC, &tmr->start);

        break;
    }
    case EV_TIMER_STOP: {
        struct po_perf_timer *tmr = NULL;
        if (e->name)
            tmr = po_hashtable_get(ctx.timers, e->name);

        if (!tmr)
            break;

        struct timespec end;
        clock_gettime(CLOCK_MONOTONIC, &end);

        uint64_t start_ns = (uint64_t)tmr->start.tv_sec * 1000000000U
            + (uint64_t)tmr->start.tv_nsec;
        uint64_t end_ns = (uint64_t)end.tv_sec * 1000000000U + (uint64_t)end.tv_nsec;
        atomic_fetch_add(&tmr->total_ns, end_ns - start_ns);

        break;
    }
    case EV_HISTOGRAM_RECORD: {
        struct po_perf_histogram *hg = NULL;
        if (e->name)
            hg = po_hashtable_get(ctx.histograms, e->name);

        if (!hg)
            break;

        for (size_t i = 0; i < hg->nbins; i++) {
            if (e->arg <= hg->bins[i]) {
                atomic_fetch_add(&hg->counts[i], 1);
                return;
            }
        }

        atomic_fetch_add(&hg->counts[hg->nbins - 1], 1);
        break;
    }
    case EV_SHUTDOWN:
        break;
    }
}

static void *perf_worker(void *_) {
    (void)_;
    void *batch[64];

    while (1) {
        ssize_t n = perf_batcher_next(ctx.ev_batcher, batch);
        if (n < 0)
            break;

        for (ssize_t i = 0; i < n; i++) {
            perf_event_t *e = batch[i];
            if (e->type == EV_SHUTDOWN)
                return NULL;

            dispatch_event(e);
            perf_zcpool_release(ctx.fmt_pool, e);
        }
    }

    return NULL;
}

static size_t next_pow2(size_t x) {
    size_t p = 1;
    while (p < x)
        p <<= 1;

    return p;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
int po_perf_init(size_t expected_counters, size_t expected_timers, size_t expected_histograms) {
    if (ctx.initialized) {
        errno = PERF_EALREADY;
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.counters = po_hashtable_create_sized(compare_strings, hash_string, expected_counters);
    if (!ctx.counters)
        goto fail;

    ctx.timers = po_hashtable_create_sized(compare_strings, hash_string, expected_timers);
    if (!ctx.timers)
        goto fail;

    ctx.histograms = po_hashtable_create_sized(compare_strings, hash_string, expected_histograms);
    if (!ctx.histograms)
        goto fail;

    ctx.event_q = perf_ringbuf_create(next_pow2(
        (expected_counters + expected_timers + expected_histograms) * 2
    ));
    if (!ctx.event_q)
        goto fail;

    ctx.ev_batcher = perf_batcher_create(ctx.event_q, 64);
    if (!ctx.ev_batcher)
        goto fail;

    ctx.fmt_pool = perf_zcpool_create(256, sizeof(perf_event_t));
    if (!ctx.fmt_pool)
        goto fail;

    if (pthread_create(&ctx.worker_tid, NULL, perf_worker, NULL) != 0)
        goto fail;

    ctx.initialized = 1;
    return 0;

fail: {
    int last_errno = errno;
    if (ctx.fmt_pool)
        perf_zcpool_destroy(&ctx.fmt_pool);

    if (ctx.ev_batcher)
        perf_batcher_destroy(&ctx.ev_batcher);

    if (ctx.event_q)
        perf_ringbuf_destroy(&ctx.event_q);

    if (ctx.counters)
        po_hashtable_destroy(&ctx.counters);

    if (ctx.timers)
        po_hashtable_destroy(&ctx.timers);

    if (ctx.histograms)
        po_hashtable_destroy(&ctx.histograms);

    memset(&ctx, 0, sizeof(ctx));
    errno = last_errno;
    return -1;
}
}

void po_perf_shutdown(FILE *out) {
    if (!ctx.initialized)
        return;

    perf_event_t *e = perf_zcpool_acquire(ctx.fmt_pool);
    if (e) {
        e->type = EV_SHUTDOWN;
        perf_batcher_enqueue(ctx.ev_batcher, e);
    }

    pthread_join(ctx.worker_tid, NULL);
    po_perf_report(out);

    if (ctx.ev_batcher)
        perf_batcher_destroy(&ctx.ev_batcher);

    if (ctx.event_q)
        perf_ringbuf_destroy(&ctx.event_q);

    if (ctx.fmt_pool)
        perf_zcpool_destroy(&ctx.fmt_pool);

    po_hashtable_iter_t *it = po_hashtable_iterator(ctx.counters);
    while (it && po_hashtable_iter_next(it)) {
        free(po_hashtable_iter_key(it));
        free(po_hashtable_iter_value(it));
    }
    free(it);

    if (ctx.counters)
        po_hashtable_destroy(&ctx.counters);

    it = po_hashtable_iterator(ctx.timers);
    while (it && po_hashtable_iter_next(it)) {
        free(po_hashtable_iter_key(it));
        free(po_hashtable_iter_value(it));
    }
    free(it);

    if (ctx.timers)
        po_hashtable_destroy(&ctx.timers);

    it = po_hashtable_iterator(ctx.histograms);
    while (it && po_hashtable_iter_next(it)) {
        free(po_hashtable_iter_key(it));
        struct po_perf_histogram *h = po_hashtable_iter_value(it);
        free(h->bins);
        free(h->counts);
        free(h);
    }
    free(it);
    if (ctx.histograms)
        po_hashtable_destroy(&ctx.histograms);

    memset(&ctx, 0, sizeof(ctx));
}

int po_perf_counter_create(const char *name) {
    if (!ctx.initialized) {
        errno = PERF_ENOTINIT;
        return -1;
    }

    perf_event_t *e = perf_zcpool_acquire(ctx.fmt_pool);
    e->type = EV_COUNTER_ADD;
    e->name = name;
    e->arg = 0;
    if (perf_batcher_enqueue(ctx.ev_batcher, e) < 0) {
        errno = PERF_EBUSY;
        return -1;
    }

    return 0;
}
void po_perf_counter_inc(const char *name) {
    if (!ctx.initialized)
        return;

    perf_event_t *e = perf_zcpool_acquire(ctx.fmt_pool);
    if (!e)
        return;

    e->type = EV_COUNTER_ADD;
    e->name = name;
    e->arg = 1;
    perf_batcher_enqueue(ctx.ev_batcher, e);
}
void po_perf_counter_add(const char *name, uint64_t delta) {
    if (!ctx.initialized)
        return;

    perf_event_t *e = perf_zcpool_acquire(ctx.fmt_pool);
    if (!e)
        return;

    e->type = EV_COUNTER_ADD;
    e->name = name;
    e->arg = delta;
    perf_batcher_enqueue(ctx.ev_batcher, e);
}

int po_perf_timer_create(const char *name) {
    if (!ctx.initialized) {
        errno = PERF_ENOTINIT;
        return -1;
    }

    struct po_perf_timer *t = po_hashtable_get(ctx.timers, name);
    if (!t) {
        t = calloc(1, sizeof(*t));
        if (!t)
            return -1;

        atomic_init(&t->total_ns, 0);
        po_hashtable_put(ctx.timers, strdup(name), t);
    }

    return 0;
}

int po_perf_timer_start(const char *name) {
    if (!ctx.initialized) {
        errno = PERF_ENOTINIT;
        return -1;
    }

    perf_event_t *e = perf_zcpool_acquire(ctx.fmt_pool);
    if (!e)
        return -1;

    e->type = EV_TIMER_START;
    e->name = name;
    return perf_batcher_enqueue(ctx.ev_batcher, e) == 0 ? 0 : -1;
}

int po_perf_timer_stop(const char *name) {
    if (!ctx.initialized) {
        errno = PERF_ENOTINIT;
        return -1;
    }

    perf_event_t *e = perf_zcpool_acquire(ctx.fmt_pool);
    if (!e) 
        return -1;

    e->type = EV_TIMER_STOP;
    e->name = name;
    return perf_batcher_enqueue(ctx.ev_batcher, e) == 0 ? 0 : -1;
}

int po_perf_histogram_create(const char *name, const uint64_t *bins, size_t nbins) {
    if (nbins == 0) {
        errno = EINVAL;
        return -1;
    }

    if (!ctx.initialized) {
        errno = PERF_ENOTINIT;
        return -1;
    }

    if (po_hashtable_contains_key(ctx.histograms, name)) {
        errno = EEXIST;
        return -1;
    }

    po_perf_histogram_t *h = malloc(sizeof(*h));
    if (!h)
        return -1;

    h->nbins = nbins;
    h->bins = malloc(nbins * sizeof(*h->bins));
    if (!h->bins) {
        free(h);
        return -1;
    }

    h->counts = calloc(nbins, sizeof(*h->counts));
    if (!h->counts) {
        free(h->bins);
        free(h);
        return -1;
    }

    memcpy(h->bins, bins, nbins * sizeof(*h->bins));
    po_hashtable_put(ctx.histograms, strdup(name), h);
    return 0;
}
int po_perf_histogram_record(const char *h, uint64_t value) {
    if (!ctx.initialized) {
        errno = PERF_ENOTINIT;
        return -1;
    }

    perf_event_t *e = perf_zcpool_acquire(ctx.fmt_pool);
    if (!e)
        return -1;

    e->type = EV_HISTOGRAM_RECORD;
    e->name = h;
    e->arg = value;
    if (perf_batcher_enqueue(ctx.ev_batcher, e) < 0)
        return -1;

    return 0;
}

int po_perf_report(FILE *out) {
    fprintf(out, "=== Performance Report ===\n");
    fprintf(out, "-- Counters --\n");
    po_hashtable_iter_t *it = po_hashtable_iterator(ctx.counters);
    while (po_hashtable_iter_next(it)) {
        fprintf(out, "%s: %lu\n", (char *)po_hashtable_iter_key(it),
                (unsigned long)atomic_load(
                    &((struct po_perf_counter *)po_hashtable_iter_value(it))->value));
    }

    free(it);
    fprintf(out, "-- Timers (ns) --\n");
    it = po_hashtable_iterator(ctx.timers);
    while (po_hashtable_iter_next(it)) {
        fprintf(out, "%s: %lu\n", (char *)po_hashtable_iter_key(it),
                (unsigned long)atomic_load(
                    &((struct po_perf_timer *)po_hashtable_iter_value(it))->total_ns));
    }

    free(it);
    fprintf(out, "-- Histograms --\n");
    it = po_hashtable_iterator(ctx.histograms);
    while (po_hashtable_iter_next(it)) {
        struct po_perf_histogram *h = po_hashtable_iter_value(it);
        fprintf(out, "%s:\n", (char *)po_hashtable_iter_key(it));
        for (size_t i = 0; i < h->nbins; i++) {
            fprintf(out, "  <= %lu: %lu\n", (unsigned long)h->bins[i],
                    (unsigned long)atomic_load(&h->counts[i]));
        }
    }

    free(it);
    return 0;
}

// Test/support API: best-effort synchronous flush of pending perf events.
// Spins briefly until the event ring appears empty or a timeout elapses.
// This does not guarantee histogram timer ordering beyond normal semantics
// but is sufficient for tests wanting to observe counter increments.
int po_perf_flush(void) {
    if (!ctx.initialized)
        return -1;

    const int max_spins = 1000; // ~ up to ~50ms worst case with backoff
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 50 * 1000}; // 50us
    int stable = 0;
    size_t last = (size_t)-1;

    for (int i = 0; i < max_spins; ++i) {
        size_t cnt = perf_ringbuf_count(ctx.event_q);
        if (cnt == 0) {
            if (stable++ > 2)
                return 0; // observed empty multiple consecutive times
        }
        else
            stable = 0;

        if (cnt == last && cnt == 0)
            return 0;

        last = cnt;
        nanosleep(&ts, NULL);
        if (ts.tv_nsec < 2 * 1000 * 1000)
            ts.tv_nsec += 50 * 1000; // gentle backoff up to 2ms
    }

    return perf_ringbuf_count(ctx.event_q) == 0 ? 0 : -1;
}
