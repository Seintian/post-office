#ifndef PO_PERF_H
#define PO_PERF_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle for a named counter. */
typedef struct perf_counter perf_counter_t;

/** Opaque handle for a named timer. */
typedef struct perf_timer   perf_timer_t;

/** Opaque handle for a named histogram. */
typedef struct perf_histogram perf_histogram_t;

/**
 * @brief Initialize the performance subsystem.
 * 
 * Must be called once at process startup before any other perf calls.
 * 
 * @param expected_counters    Number of distinct counters you will register.
 * @param expected_timers      Number of distinct timers you will register.
 * @param expected_histograms  Number of histograms you will register.
 * @return 0 on success, non‐zero on error.
 */
int perf_init(
    size_t expected_counters,
    size_t expected_timers,
    size_t expected_histograms
);

/**
 * @brief Shutdown the perf subsystem, releasing all resources.
 */
void perf_shutdown(void);

/**
 * @brief Create or look up a named counter.
 *
 * @param name  Unique ASCII name for this counter.
 * @return pointer to perf_counter_t, or NULL on failure.
 */
perf_counter_t *perf_counter_create(const char *name);

/**
 * @brief Atomically increment a counter by 1.
 * 
 * @param ctr  Counter handle.
 */
void perf_counter_inc(perf_counter_t *ctr);

/**
 * @brief Atomically add a value to a counter.
 * 
 * @param ctr   Counter handle.
 * @param delta Amount (>=0) to add.
 */
void perf_counter_add(perf_counter_t *ctr, uint64_t delta);

/**
 * @brief Read the current value of a counter.
 * 
 * @param ctr  Counter handle.
 * @return current count.
 */
uint64_t perf_counter_value(perf_counter_t *ctr);

/**
 * @brief Create or look up a named high‑resolution timer.
 *
 * @param name  Unique ASCII name for this timer.
 * @return pointer to perf_timer_t, or NULL on failure.
 */
perf_timer_t *perf_timer_create(const char *name);

/**
 * @brief Record a start timestamp for a timer.
 *
 * @param t  Timer handle.
 */
int perf_timer_start(perf_timer_t *t);

/**
 * @brief Record an end timestamp for a timer and accumulate the interval.
 *
 * @param t  Timer handle.
 */
int perf_timer_stop(perf_timer_t *t);

/**
 * @brief Get the total accumulated nanoseconds for a timer.
 *
 * @param t  Timer handle.
 * @return total time in nanoseconds.
 */
uint64_t perf_timer_ns(perf_timer_t *t);

/**
 * @brief Create or look up a named histogram with fixed bins.
 *
 * @param name   Unique ASCII name for this histogram.
 * @param bins   Array of bin upper‐bounds (monotonic increasing).
 * @param nbins  Number of bins.
 * @return pointer to perf_histogram_t, or NULL on failure.
 */
perf_histogram_t *perf_histogram_create(
    const char *name,
    const uint64_t *bins,
    size_t nbins
);

/**
 * @brief Record a value into the histogram.
 *
 * @param h     Histogram handle.
 * @param value Value to bucket.
 */
void perf_histogram_record(const perf_histogram_t *h, uint64_t value);

/**
 * @brief Dump all registered counters, timers, and histograms to stdout.
 *
 * Useful at shutdown or on‐demand to get a snapshot of performance metrics.
 */
void perf_report(void);

#ifdef __cplusplus
}
#endif

#endif // PO_PERF_H
