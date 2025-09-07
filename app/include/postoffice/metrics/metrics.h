/** \file metrics.h
 *  \ingroup metrics
 *  Lightweight macro facade over perf subsystem providing fast, low-friction
 *  instrumentation points for core subsystems (logstore, logger, net, storage).
 *
 *  Design Goals:
 *   - Zero runtime cost when disabled (compile-out via PO_METRICS_DISABLED)
 *   - Indirect through perf API only (no direct data structure exposure)
 *   - String literals used as metric identifiers (deduped by perf hashtable)
 *   - Header-only macros to avoid function call overhead on hot paths
 *
 *  Usage:
 *    // one‑time init early in process (after perf init):
 *    po_metrics_init();
 *    // increment counter
 *    PO_METRIC_COUNTER_INC("logstore.append.total");
 *    // add arbitrary delta
 *    PO_METRIC_COUNTER_ADD("logstore.append.bytes", bytes);
 *    // timers (create once, then start/stop pairs)
 *    PO_METRIC_TIMER_CREATE("logstore.flush.ns");
 *    PO_METRIC_TIMER_START("logstore.flush.ns");
 *    ... work ...
 *    PO_METRIC_TIMER_STOP("logstore.flush.ns");
 *    // histograms (define bins once then record values)
 *    static const uint64_t flush_bins[] = {1000,5000,10000,50000,100000,500000,1000000,5000000,10000000};
 *    PO_METRIC_HISTO_CREATE("logstore.flush.latency", flush_bins, sizeof(flush_bins)/sizeof(flush_bins[0]));
 *    PO_METRIC_HISTO_RECORD("logstore.flush.latency", elapsed_ns);
 *
 *  All macros are no‑ops if perf subsystem not initialized or metrics disabled.
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

#define PO_METRIC_TICK(var) po_metric_tick_t var = { po_metric_now_ns() }
#define PO_METRIC_ELAPSED_NS(var) (po_metric_now_ns() - (var).ns)

#ifdef PO_METRICS_DISABLED
// Compile-time disabled: expand to empty
#define PO_METRIC_COUNTER_INC(name)             do { (void)(name); } while (0)
#define PO_METRIC_COUNTER_ADD(name, d)          do { (void)(name); (void)(d); } while (0)
#define PO_METRIC_TIMER_CREATE(name)            do { (void)(name); } while (0)
#define PO_METRIC_TIMER_START(name)             do { (void)(name); } while (0)
#define PO_METRIC_TIMER_STOP(name)              do { (void)(name); } while (0)
#define PO_METRIC_HISTO_CREATE(name,bins,nbins) do { (void)(name); (void)(bins); (void)(nbins); } while (0)
#define PO_METRIC_HISTO_RECORD(name,val)        do { (void)(name); (void)(val); } while (0)
#else
// Active implementation delegates to perf.*
#define PO_METRIC_COUNTER_INC(name)             po_perf_counter_inc((name))
#define PO_METRIC_COUNTER_ADD(name, d)          po_perf_counter_add((name), (uint64_t)(d))
#define PO_METRIC_TIMER_CREATE(name)            po_perf_timer_create((name))
#define PO_METRIC_TIMER_START(name)             po_perf_timer_start((name))
#define PO_METRIC_TIMER_STOP(name)              po_perf_timer_stop((name))
#define PO_METRIC_HISTO_CREATE(name,bins,nbins) po_perf_histogram_create((name),(bins),(nbins))
#define PO_METRIC_HISTO_RECORD(name,val)        po_perf_histogram_record((name),(uint64_t)(val))
#endif

#ifdef __cplusplus
} // extern C
#endif

#endif // PO_METRICS_H
