#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "perf/perf.h"
#include "hashtable/hashtable.h"
#include "utils/errors.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include <errno.h>


// Internal structures
struct perf_counter {
    atomic_uint_fast64_t value;
};

struct perf_timer {
    struct timespec start;
    atomic_uint_fast64_t total_ns;
};

struct perf_histogram {
    atomic_uint_fast64_t *bins;
    atomic_uint_fast64_t *counts;
    size_t nbins;
};

// Global context
static struct {
    hashtable_t *counters;
    hashtable_t *timers;
    hashtable_t *histograms;
    int initialized;
} ctx = {0};

static unsigned long hash_string(const void *key) {
    const unsigned char *str = key;
    unsigned long hash = 5381;
    unsigned char c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

static int compare_strings(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int perf_init(
    size_t expected_counters,
    size_t expected_timers,
    size_t expected_histograms
) {
    if (ctx.initialized) {
        errno = PERF_EALREADY;
        return -1;
    }

    ctx.counters   = hashtable_create_sized(compare_strings, hash_string, expected_counters);
    ctx.timers     = hashtable_create_sized(compare_strings, hash_string, expected_timers);
    ctx.histograms = hashtable_create_sized(compare_strings, hash_string, expected_histograms);
    if (!ctx.counters || !ctx.timers || !ctx.histograms) {
        hashtable_free(ctx.counters);
        hashtable_free(ctx.timers);
        hashtable_free(ctx.histograms);

        ctx.counters = ctx.timers = ctx.histograms = NULL;
        return -1;
    }

    ctx.initialized = 1;
    return 0;
}

void perf_shutdown(void) {
    if (!ctx.initialized)
        return;

    // Destroy counters
    hashtable_iter_t *cit = hashtable_iterator(ctx.counters);
    while (hashtable_iter_next(cit)) {
        struct perf_counter *ctr = hashtable_iter_value(cit);
        free(ctr);
    }
    hashtable_free(ctx.counters);

    // Destroy timers
    hashtable_iter_t *tit = hashtable_iterator(ctx.timers);
    while (hashtable_iter_next(tit)) {
        struct perf_timer *t = hashtable_iter_value(tit);
        free(t);
    }
    hashtable_free(ctx.timers);

    // Destroy histograms
    hashtable_iter_t *hit = hashtable_iterator(ctx.histograms);
    while (hashtable_iter_next(hit)) {
        struct perf_histogram *h = hashtable_iter_value(hit);
        free(h->bins);
        free(h->counts);
        free(h);
    }
    hashtable_free(ctx.histograms);

    ctx.initialized = 0;
}

// Counters
perf_counter_t *perf_counter_create(const char *name) {
    if (!ctx.initialized) {
        errno = PERF_ENOTINIT;
        return NULL;
    }

    struct perf_counter *ctr = hashtable_get(ctx.counters, name);
    if (ctr)
        return ctr;

    ctr = calloc(1, sizeof(*ctr));
    if (!ctr)
        return NULL;

    atomic_init(&ctr->value, 0);
    if (hashtable_put(ctx.counters, strdup(name), ctr) < 0) {
        free(ctr);
        return NULL;
    }

    return ctr;
}

void perf_counter_inc(perf_counter_t *c) {
    atomic_fetch_add(&c->value, 1);
}

void perf_counter_add(perf_counter_t *c, uint64_t delta) {
    atomic_fetch_add(&c->value, delta);
}

uint64_t perf_counter_value(perf_counter_t *c) {
    return atomic_load(&c->value);
}

// Timers
perf_timer_t *perf_timer_create(const char *name) {
    if (!ctx.initialized) {
        errno = PERF_ENOTINIT;
        return NULL;
    }

    struct perf_timer *t = hashtable_get(ctx.timers, name);
    if (t)
        return t;

    t = calloc(1, sizeof(*t));
    if (!t)
        return NULL;

    atomic_init(&t->total_ns, 0);
    if (hashtable_put(ctx.timers, strdup(name), t) < 0) {
        free(t);
        return NULL;
    }
    return t;
}

int perf_timer_start(perf_timer_t *t) {
    return clock_gettime(CLOCK_MONOTONIC, &t->start);
}

int perf_timer_stop(perf_timer_t *t) {
    struct timespec end;
    if (clock_gettime(CLOCK_MONOTONIC, &end) < 0)
        return -1;

    uint64_t start_ns = (uint64_t)t->start.tv_sec * 1000000000UL + (uint64_t)t->start.tv_nsec;
    uint64_t end_ns   = (uint64_t)end.tv_sec * 1000000000UL + (uint64_t)end.tv_nsec;
    atomic_fetch_add(&t->total_ns, end_ns - start_ns);

    return 0;
}

uint64_t perf_timer_ns(perf_timer_t *t) {
    return atomic_load(&t->total_ns);
}

// Histograms
perf_histogram_t *perf_histogram_create(
    const char *name,
    const uint64_t *bins,
    size_t nbins
) {
    if (!ctx.initialized)
        return NULL;

    struct perf_histogram *h = hashtable_get(ctx.histograms, name);
    if (h)
        return h;

    h = malloc(sizeof(*h));
    if (!h)
        return NULL;

    h->nbins = nbins;
    h->bins = malloc(nbins * sizeof(atomic_uint_fast64_t));
    h->counts = calloc(nbins, sizeof(atomic_uint_fast64_t));
    if (!h->bins || !h->counts) {
        free(h->bins);
        free(h->counts);
        free(h);
        return NULL;
    }

    memcpy(h->bins, bins, nbins * sizeof(atomic_uint_fast64_t));
    if (hashtable_put(ctx.histograms, strdup(name), h) < 0) {
        free(h->bins);
        free(h->counts);
        free(h);
        return NULL;
    }

    return h;
}

void perf_histogram_record(const perf_histogram_t *h, uint64_t value) {
    // find first bin >= value
    for (size_t i = 0; i < h->nbins; i++) {
        if (value <= h->bins[i]) {
            atomic_fetch_add(&h->counts[i], 1);
            return;
        }
    }

    // overflow bin
    atomic_fetch_add(&h->counts[h->nbins - 1], 1);
}

// Reporting
void perf_report(void)
{
    printf("=== Performance Report ===\n");
    // Counters
    printf("-- Counters --\n");
    hashtable_iter_t *cit = hashtable_iterator(ctx.counters);
    while (hashtable_iter_next(cit)) {
        const char *name = hashtable_iter_key(cit);
        struct perf_counter *ctr = hashtable_iter_value(cit);
        printf("%s: %lu\n", name, (unsigned long)perf_counter_value(ctr));
    }

    // Timers
    printf("-- Timers (ns) --\n");
    hashtable_iter_t *tit = hashtable_iterator(ctx.timers);
    while (hashtable_iter_next(tit)) {
        const char *name = hashtable_iter_key(tit);
        struct perf_timer *t = hashtable_iter_value(tit);
        printf("%s: %lu\n", name, (unsigned long)perf_timer_ns(t));
    }

    // Histograms
    printf("-- Histograms --\n");
    hashtable_iter_t *hit = hashtable_iterator(ctx.histograms);
    while (hashtable_iter_next(hit)) {
        const char *name = hashtable_iter_key(hit);
        struct perf_histogram *h = hashtable_iter_value(hit);
        printf("%s:\n", name);
        for (size_t i = 0; i < h->nbins; i++) {
            printf("  <= %lu: %lu\n",
                   (unsigned long)h->bins[i], (unsigned long)h->counts[i]);
        }
    }
}
