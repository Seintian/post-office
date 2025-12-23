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
 * @brief Initialize the perf module and spawn background worker thread.
 *
 * Sets up shared memory and initializes global structures.
 *
 * @param[in] expected_counters   Expected number of counters to pre-size the hashtable.
 * @param[in] expected_timers     Expected number of timers.
 * @param[in] expected_histograms Expected number of histograms.
 * @return 0 on success, -1 on failure (errno set).
 *
 * @note Thread-safe: No (Must be called once during startup).
 */
int po_perf_init(size_t expected_counters, size_t expected_timers, size_t expected_histograms);

/**
 * @brief Gracefully shut down perf, flush events, destroy tables and report stats.
 *
 * @param[in] out Output stream for final report (Nullable).
 *
 * @note Thread-safe: No (Must be called once during shutdown).
 */
void po_perf_shutdown(FILE *out);

// -----------------------------------------------------------------------------
// Counters API
// -----------------------------------------------------------------------------

/**
 * @brief Create or retrieve a counter by name (synchronous).
 *
 * @param[in] name Counter name (string key, must not be NULL).
 * @return 0 on success, -1 on failure (errno set).
 *
 * @note Thread-safe: Yes (Global lock or lock-free hash table).
 */
int po_perf_counter_create(const char *name) __nonnull((1));

/**
 * @brief Increment counter value by 1 asynchronously.
 *
 * @param[in] name Counter name (string key, must not be NULL).
 *
 * @note Thread-safe: Yes (Wait-free).
 */
void po_perf_counter_inc(const char *name) __nonnull((1));

/**
 * @brief Add delta to counter value asynchronously.
 *
 * @param[in] name  Counter name (string key, must not be NULL).
 * @param[in] delta Amount to add.
 *
 * @note Thread-safe: Yes (Wait-free).
 */
void po_perf_counter_add(const char *name, uint64_t delta) __nonnull((1));

// -----------------------------------------------------------------------------
// Timers API
// -----------------------------------------------------------------------------

/**
 * @brief Create or retrieve a timer by name (synchronous).
 *
 * @param[in] name Timer name (string key, must not be NULL).
 * @return 0 on success, -1 on failure (errno set).
 *
 * @note Thread-safe: Yes.
 */
int po_perf_timer_create(const char *name) __nonnull((1));

/**
 * @brief Start a timer asynchronously.
 *
 * @param[in] name Timer name (string key, must not be NULL).
 * @return 0 on success, -1 on failure.
 *
 * @note Thread-safe: Yes.
 */
int po_perf_timer_start(const char *name) __nonnull((1));

/**
 * @brief Stop a timer asynchronously and accumulate elapsed time.
 *
 * @param[in] name Timer name (string key, must not be NULL).
 * @return 0 on success, -1 on failure.
 *
 * @note Thread-safe: Yes.
 */
int po_perf_timer_stop(const char *name) __nonnull((1));

// -----------------------------------------------------------------------------
// Histograms API
// -----------------------------------------------------------------------------

/**
 * @brief Create a histogram by name (synchronous).
 *
 * @param[in] name  Name of histogram (must not be NULL).
 * @param[in] bins  Array of bin thresholds (must not be NULL).
 * @param[in] nbins Number of bins.
 * @return 0 on success, -1 on failure (errno set).
 *
 * @note Thread-safe: Yes.
 */
int po_perf_histogram_create(const char *name, const uint64_t *bins, size_t nbins)
    __nonnull((1, 2));

/**
 * @brief Record a value asynchronously into the histogram.
 *
 * @param[in] name  Histogram name (string key, must not be NULL).
 * @param[in] value Value to record.
 * @return 0 on success, -1 on failure (errno set).
 *
 * @note Thread-safe: Yes.
 */
int po_perf_histogram_record(const char *name, uint64_t value) __nonnull((1));

// -----------------------------------------------------------------------------
// Lookup Functions (for macro caching)
// -----------------------------------------------------------------------------

/**
 * @brief Lookup or allocate a counter by name, returning its index.
 *
 * Used by macros for TLS-based index caching.
 *
 * @param[in] name Counter name (must not be NULL).
 * @return Index >= 0 on success, -1 on failure.
 *
 * @note Thread-safe: Yes.
 */
int po_perf_counter_lookup(const char *name) __nonnull((1));

/**
 * @brief Lookup or allocate a timer by name, returning its index.
 *
 * @param[in] name Timer name (must not be NULL).
 * @return Index >= 0 on success, -1 on failure.
 *
 * @note Thread-safe: Yes.
 */
int po_perf_timer_lookup(const char *name) __nonnull((1));

/**
 * @brief Lookup a histogram by name, returning its index.
 *
 * @param[in] name Histogram name (must not be NULL).
 * @return Index >= 0 if found, -1 if not found.
 *
 * @note Thread-safe: Yes.
 */
int po_perf_histogram_lookup(const char *name) __nonnull((1));

// -----------------------------------------------------------------------------
// Fast-Path Functions (operate on indices, for macro caching)
// -----------------------------------------------------------------------------

/**
 * @brief Increment counter by index (no lookup overhead).
 *
 * @param[in] idx Counter index from po_perf_counter_lookup.
 *
 * @note Thread-safe: Yes (Wait-free).
 */
void po_perf_counter_inc_by_idx(int idx);

/**
 * @brief Add delta to counter by index.
 *
 * @param[in] idx Counter index.
 * @param[in] delta Amount to add.
 *
 * @note Thread-safe: Yes (Wait-free).
 */
void po_perf_counter_add_by_idx(int idx, uint64_t delta);

/**
 * @brief Start timer by index.
 *
 * @param[in] idx Timer index from po_perf_timer_lookup.
 *
 * @note Thread-safe: Yes.
 */
void po_perf_timer_start_by_idx(int idx);

/**
 * @brief Stop timer by index.
 *
 * @param[in] idx Timer index.
 *
 * @note Thread-safe: Yes.
 */
void po_perf_timer_stop_by_idx(int idx);

/**
 * @brief Record histogram value by index.
 *
 * @param[in] idx Histogram index from po_perf_histogram_lookup.
 * @param[in] value Value to record.
 *
 * @note Thread-safe: Yes.
 */
void po_perf_histogram_record_by_idx(int idx, uint64_t value);

// -----------------------------------------------------------------------------
// Reporting
// -----------------------------------------------------------------------------

/**
 * @brief Print a synchronous report of all counters, timers, and histograms.
 *
 * This function does not queue an event; it runs synchronously.
 *
 * @param[in] out  Output stream (e.g., stdout) (Nullable, defaults to stdout if NULL).
 * @return 0 on success, -1 on failure.
 *
 * @note Thread-safe: Yes (Internal locking).
 */
int po_perf_report(FILE *out);

/**
 * @brief Best-effort synchronous flush of queued perf events.
 *
 * Attempts to drain the ringbuffer so that counters / timers become visible
 * to po_perf_report without waiting for shutdown.
 *
 * @return 0 on apparent success (queue drained) or -1 if still pending after retries.
 *
 * @note Thread-safe: Yes.
 */
int po_perf_flush(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _PO_PERF_H
