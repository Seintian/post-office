/** \file metrics.h
 *  \ingroup metrics
 *  @brief Lightweight macro facade over the perf subsystem providing fast,
 *         low-friction instrumentation (counters, timers, histograms) used by
 *         core subsystems (logstore, logger, net, storage, etc.).
 *
 *  Rationale
 *  ---------
 *  Direct calls into the perf API add cognitive weight and risk accidental
 *  misuse (e.g., forgetting to create a metric before incrementing). This
 *  facade centralizes common usage patterns into zero/low-overhead macros that
 *  safely compile out when metrics are disabled at build time.
 *
 *  Design Goals
 *  ------------
 *  - Zero runtime cost when disabled (compile-out via `PO_METRICS_DISABLED`).
 *  - Indirection only through perf API (no internal structure leakage).
 *  - String literal (const char*) names; deduped internally by perf hashtable.
 *  - Header-only macros: avoid function call / branch overhead on hot paths.
 *  - Safe to invoke before explicit creation (create-on-first-use for timers /
 *    counters / histograms via underlying perf calls where semantics allow).
 *
 *  Thread Safety & Ordering
 *  ------------------------
 *  All macros delegate to perf asynchronous recording functions that are
 *  thread-safe. Timer start/stop pairings are logical (identified by name) and
 *  should be balanced from the same thread to preserve intuitive semantics.
 *  Histogram recording is fire-and-forget; bins are defined once.
 *
 *  Initialization / Shutdown
 *  -------------------------
 *  Call `po_perf_init()` early with sizing hints, then `po_metrics_init()` to
 *  perform any facade-specific setup (currently light-weight). On shutdown
 *  call `po_metrics_shutdown()` followed by `po_perf_shutdown()` to flush.
 *
 *  Compile-Out Behavior
 *  --------------------
 *  Defining `PO_METRICS_DISABLED` at compile time replaces all macros with
 *  no-op expressions that still evaluate and cast arguments (to avoid unused
 *  warnings) but generate no code. This guarantees zero overhead in builds
 *  where metrics are undesired (e.g., minimal production footprint tests).
 *
 *  Error Handling
 *  --------------
 *  Macros themselves do not surface errors. Underlying perf create operations
 *  may set `errno` on allocation failure, but the facade intentionally does not
 *  branch on those conditions to keep hot paths lean. Reporting still omits
 *  failed metrics gracefully.
 *
 *  Usage Example
 *  -------------
 *  @code
 *  // After po_perf_init(...):
 *  po_metrics_init();
 *  PO_METRIC_COUNTER_INC("logstore.append.total");
 *  PO_METRIC_COUNTER_ADD("logstore.append.bytes", bytes);
 *  PO_METRIC_TIMER_CREATE("logstore.flush.ns");
 *  PO_METRIC_TIMER_START("logstore.flush.ns");
 *  // ... critical section ...
 *  PO_METRIC_TIMER_STOP("logstore.flush.ns");
 *  static const uint64_t flush_bins[] = {1000,5000,10000,50000,100000,500000,1000000,5000000,10000000};
 *  PO_METRIC_HISTO_CREATE("logstore.flush.latency", flush_bins, sizeof(flush_bins)/sizeof(flush_bins[0]));
 *  PO_METRIC_HISTO_RECORD("logstore.flush.latency", elapsed_ns);
 *  @endcode
 *
 *  Cross-References
 *  ----------------
 *  @see perf/perf.h for underlying primitive operations
 *  @see zerocopy.h for performance-sensitive pools often instrumented
 *  @see logstore.h for append / flush metrics usage patterns
 *  @see ringbuf.h For structures commonly measured by counters.
 *
 *  All macros are no-ops if the perf subsystem is not initialized or metrics
 *  are disabled at compile time.
 */
#ifndef PO_METRICS_H
#define PO_METRICS_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <time.h>

#include "perf/perf.h"

#ifdef __cplusplus
extern "C" {
#endif

int po_metrics_init(void);
void po_metrics_shutdown(void);

/* Lightweight per-scope timing helper for histogram recording without
 * depending on perf internals for elapsed retrieval. Usage:
 *   PO_METRIC_LATENCY_SCOPE(start_ts);
 *   ... work ...
 *   uint64_t dur = PO_METRIC_LATENCY_ELAPSED_NS(start_ts);
 */
typedef struct {
    uint64_t ns;
} po_metric_tick_t;

static inline uint64_t po_metric_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

#define PO_METRIC_TICK(var) po_metric_tick_t var = {po_metric_now_ns()}
#define PO_METRIC_ELAPSED_NS(var) (po_metric_now_ns() - (var).ns)

#ifdef PO_METRICS_DISABLED
// Compile-time disabled: expand to empty
#define PO_METRIC_COUNTER_CREATE(name)                                                             \
    do {                                                                                           \
        (void)(name);                                                                              \
    } while (0)
#define PO_METRIC_COUNTER_INC(name)                                                                \
    do {                                                                                           \
        (void)(name);                                                                              \
    } while (0)
#define PO_METRIC_COUNTER_ADD(name, d)                                                             \
    do {                                                                                           \
        (void)(name);                                                                              \
        (void)(d);                                                                                 \
    } while (0)
#define PO_METRIC_TIMER_CREATE(name)                                                               \
    do {                                                                                           \
        (void)(name);                                                                              \
    } while (0)
#define PO_METRIC_TIMER_START(name)                                                                \
    do {                                                                                           \
        (void)(name);                                                                              \
    } while (0)
#define PO_METRIC_TIMER_STOP(name)                                                                 \
    do {                                                                                           \
        (void)(name);                                                                              \
    } while (0)
#define PO_METRIC_HISTO_CREATE(name, bins, nbins)                                                  \
    do {                                                                                           \
        (void)(name);                                                                              \
        (void)(bins);                                                                              \
        (void)(nbins);                                                                             \
    } while (0)
#define PO_METRIC_HISTO_RECORD(name, val)                                                          \
    do {                                                                                           \
        (void)(name);                                                                              \
        (void)(val);                                                                               \
    } while (0)
#else
// Active implementation delegates to perf.*
#define PO_METRIC_COUNTER_CREATE(name) po_perf_counter_create((name))
#define PO_METRIC_COUNTER_INC(name) po_perf_counter_inc((name))
#define PO_METRIC_COUNTER_ADD(name, d) po_perf_counter_add((name), (uint64_t)(d))
#define PO_METRIC_TIMER_CREATE(name) po_perf_timer_create((name))
#define PO_METRIC_TIMER_START(name) po_perf_timer_start((name))
#define PO_METRIC_TIMER_STOP(name) po_perf_timer_stop((name))
#define PO_METRIC_HISTO_CREATE(name, bins, nbins) po_perf_histogram_create((name), (bins), (nbins))
#define PO_METRIC_HISTO_RECORD(name, val) po_perf_histogram_record((name), (uint64_t)(val))
#endif

#ifdef __cplusplus
} // extern C
#endif

#endif // PO_METRICS_H
