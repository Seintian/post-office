#ifndef _PO_PERF_H
/** \ingroup perf */
#define _PO_PERF_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "utils/errors.h"


// -----------------------------------------------------------------------------
// Public opaque types
// -----------------------------------------------------------------------------
typedef struct perf_counter   perf_counter_t;
typedef struct perf_timer     perf_timer_t;
typedef struct perf_histogram perf_histogram_t;

// -----------------------------------------------------------------------------
// Initialization / Shutdown
// -----------------------------------------------------------------------------

/**
 * Initialize the perf module and spawn background worker thread.
 *
 * @param expected_counters   Expected number of counters to pre-size the hashtable
 * @param expected_timers     Expected number of timers
 * @param expected_histograms Expected number of histograms
 * @return 0 on success, -1 on failure (errno set)
 */
int perf_init(
    size_t expected_counters,
    size_t expected_timers,
    size_t expected_histograms
);

/**
 * Gracefully shut down perf, flush events, destroy tables and report stats.
 */
void perf_shutdown(FILE *out);

// -----------------------------------------------------------------------------
// Counters API
// -----------------------------------------------------------------------------

/**
 * Create or retrieve a counter by name (synchronous).
 *
 * @param name  Counter name (string key)
 * @return 0 on success, -1 on failure (errno set)
 */
int perf_counter_create(const char *name) __nonnull((1));

/**
 * Increment counter value by 1 asynchronously.
 *
 * @param name  Counter name (string key)
 */
void perf_counter_inc(const char *name) __nonnull((1));

/**
 * Add delta to counter value asynchronously.
 *
 * @param name   Counter name (string key)
 * @param delta  Amount to add
 */
void perf_counter_add(const char *name, uint64_t delta) __nonnull((1));

// -----------------------------------------------------------------------------
// Timers API
// -----------------------------------------------------------------------------

/**
 * Create or retrieve a timer by name (synchronous).
 *
 * @param name Timer name (string key)
 * @return 0 on success, -1 on failure (errno set)
 */
int perf_timer_create(const char *name) __nonnull((1));

/**
 * Start a timer asynchronously.
 *
 * @param name  Timer name (string key)
 * @return 0 on success, -1 on failure
 */
int perf_timer_start(const char *name) __nonnull((1));

/**
 * Stop a timer asynchronously and accumulate elapsed time.
 *
 * @param name Timer name (string key)
 * @return 0 on success, -1 on failure
 */
int perf_timer_stop(const char *name) __nonnull((1));

// -----------------------------------------------------------------------------
// Histograms API
// -----------------------------------------------------------------------------

/**
 * Create a histogram by name (synchronous).
 *
 * @param name  Name of histogram
 * @param bins  Array of bin thresholds
 * @param nbins Number of bins
 * @return 0 on success, -1 on failure (errno set)
 */
int perf_histogram_create(
    const char *name,
    const uint64_t *bins,
    size_t nbins
) __nonnull((1, 2));

/**
 * Record a value asynchronously into the histogram.
 *
 * @param name  Histogram name (string key)
 * @param value Value to record
 * @return 0 on success, -1 on failure (errno set)
 */
int perf_histogram_record(
    const char *name,
    uint64_t value
) __nonnull((1));

// -----------------------------------------------------------------------------
// Reporting
// -----------------------------------------------------------------------------

/**
 * Print a synchronous report of all counters, timers, and histograms.
 * This function does not queue an event; it runs synchronously.
 *
 * @param out  Output stream (e.g., stdout)
 * @return 0 on success, -1 on failure
 */
int perf_report(FILE *out);

#endif // _PO_PERF_H
