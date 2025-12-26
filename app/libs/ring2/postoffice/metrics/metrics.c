#include "metrics/metrics.h"
#include "perf/perf.h"

// Default capacity for metrics subsystem
// These values accommodate typical application usage:
// - 64 counters (37 currently used + headroom)
// - 16 timers (4 currently used + headroom)
// - 8 histograms (5 currently used + headroom)
#define METRICS_DEFAULT_COUNTERS 64
#define METRICS_DEFAULT_TIMERS 16
#define METRICS_DEFAULT_HISTOGRAMS 8

int po_metrics_init(int counters, int timers, int histograms) {
    return po_perf_init(
        (size_t)(counters > 0 ? counters : METRICS_DEFAULT_COUNTERS), 
        (size_t)(timers > 0 ? timers : METRICS_DEFAULT_TIMERS), 
        (size_t)(histograms > 0 ? histograms : METRICS_DEFAULT_HISTOGRAMS)
    );
}

void po_metrics_shutdown(void) {
    po_perf_shutdown(NULL);
}
