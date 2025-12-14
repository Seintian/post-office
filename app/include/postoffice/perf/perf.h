/**
 * @file perf.h
 * @ingroup perf
 * @brief Low-level performance instrumentation primitives (counters, timers,
 *        histograms) using shared memory for global cross-process metrics.
 *
 * Architecture
 * -----------
 *  - Uses POSIX Shared Memory (SHM) to store metrics globally.
 *  - Atomic operations for lock-free updates (wait-free for counters).
 *  - No internal dynamic allocation or complex structures (hashtables) to
 *    avoid recursion loops when instrumenting core libraries.
 *
 * Threading
 * ---------
 * Public API functions are thread-safe and process-safe. Initialization must
 * occur in every process (handles SHM mapping).
 *
 * Error Handling
 * --------------
 * Creation operations return -1 with errno set (ENOMEM, EINVAL) on failure.
 * Increment / record operations are void or best-effort (silently drop if
 * initialization incomplete).
 *
 * @see metrics.h Header-only macro facade for lightweight usage.
 */
#ifndef _PO_PERF_H
#define _PO_PERF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "utils/errors.h"

// -----------------------------------------------------------------------------
// Public opaque types (canonical po_ names)
// -----------------------------------------------------------------------------
typedef struct po_perf_counter po_perf_counter_t;     // opaque
typedef struct po_perf_timer po_perf_timer_t;         // opaque
typedef struct po_perf_histogram po_perf_histogram_t; // opaque

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
int po_perf_init(size_t expected_counters, size_t expected_timers, size_t expected_histograms);

/**
 * Gracefully shut down perf, flush events, destroy tables and report stats.
 */
void po_perf_shutdown(FILE *out);

// -----------------------------------------------------------------------------
// Counters API
// -----------------------------------------------------------------------------

/**
 * Create or retrieve a counter by name (synchronous).
 *
 * @param name  Counter name (string key)
 * @return 0 on success, -1 on failure (errno set)
 */
int po_perf_counter_create(const char *name) __nonnull((1));

/**
 * Increment counter value by 1 asynchronously.
 *
 * @param name  Counter name (string key)
 */
void po_perf_counter_inc(const char *name) __nonnull((1));

/**
 * Add delta to counter value asynchronously.
 *
 * @param name   Counter name (string key)
 * @param delta  Amount to add
 */
void po_perf_counter_add(const char *name, uint64_t delta) __nonnull((1));

// -----------------------------------------------------------------------------
// Timers API
// -----------------------------------------------------------------------------

/**
 * Create or retrieve a timer by name (synchronous).
 *
 * @param name Timer name (string key)
 * @return 0 on success, -1 on failure (errno set)
 */
int po_perf_timer_create(const char *name) __nonnull((1));

/**
 * Start a timer asynchronously.
 *
 * @param name  Timer name (string key)
 * @return 0 on success, -1 on failure
 */
int po_perf_timer_start(const char *name) __nonnull((1));

/**
 * Stop a timer asynchronously and accumulate elapsed time.
 *
 * @param name Timer name (string key)
 * @return 0 on success, -1 on failure
 */
int po_perf_timer_stop(const char *name) __nonnull((1));

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
int po_perf_histogram_create(const char *name, const uint64_t *bins, size_t nbins)
    __nonnull((1, 2));

/**
 * Record a value asynchronously into the histogram.
 *
 * @param name  Histogram name (string key)
 * @param value Value to record
 * @return 0 on success, -1 on failure (errno set)
 */
int po_perf_histogram_record(const char *name, uint64_t value) __nonnull((1));

// -----------------------------------------------------------------------------
// Lookup Functions (for macro caching)
// -----------------------------------------------------------------------------

/**
 * Lookup or allocate a counter by name, returning its index.
 * Used by macros for TLS-based index caching.
 * @param name Counter name
 * @return Index >= 0 on success, -1 on failure
 */
int po_perf_counter_lookup(const char *name) __nonnull((1));

/**
 * Lookup or allocate a timer by name, returning its index.
 * @param name Timer name
 * @return Index >= 0 on success, -1 on failure
 */
int po_perf_timer_lookup(const char *name) __nonnull((1));

/**
 * Lookup a histogram by name, returning its index.
 * @param name Histogram name
 * @return Index >= 0 if found, -1 if not found
 */
int po_perf_histogram_lookup(const char *name) __nonnull((1));

// -----------------------------------------------------------------------------
// Fast-Path Functions (operate on indices, for macro caching)
// -----------------------------------------------------------------------------

/**
 * Increment counter by index (no lookup overhead).
 * @param idx Counter index from po_perf_counter_lookup
 */
void po_perf_counter_inc_by_idx(int idx);

/**
 * Add delta to counter by index.
 * @param idx Counter index
 * @param delta Amount to add
 */
void po_perf_counter_add_by_idx(int idx, uint64_t delta);

/**
 * Start timer by index.
 * @param idx Timer index from po_perf_timer_lookup
 */
void po_perf_timer_start_by_idx(int idx);

/**
 * Stop timer by index.
 * @param idx Timer index
 */
void po_perf_timer_stop_by_idx(int idx);

/**
 * Record histogram value by index.
 * @param idx Histogram index from po_perf_histogram_lookup
 * @param value Value to record
 */
void po_perf_histogram_record_by_idx(int idx, uint64_t value);

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
int po_perf_report(FILE *out);

/**
 * Best-effort synchronous flush of queued perf events so that counters / timers
 * become visible to po_perf_report without waiting for shutdown. Returns 0 on
 * apparent success (queue drained) or -1 if still pending after retries.
 */
int po_perf_flush(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _PO_PERF_H
