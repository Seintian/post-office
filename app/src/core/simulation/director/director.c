// Director process: collects system info and (now) initializes performance
// instrumentation so that counters, timers and histograms accumulated by any
// library code used during its lifetime can be reported at shutdown.

#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>
#include <utils/errors.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    // Initialize perf first (metrics macros depend on perf being initialized)
    if (po_perf_init(8, 8, 4) != 0) { // heuristic capacities; adjust if needed
        fprintf(stderr, "director: perf init failed: %s\n", po_strerror(errno));
        // continue without perf/metrics rather than aborting entirely
    } else {
        // Initialize metrics wrapper (lightweight, may be a no-op if already)
        if (po_metrics_init() != 0) {
            fprintf(stderr, "director: metrics init failed: %s\n", po_strerror(errno));
        }
    }

    // Director main loop would go here (or sleep for now)
    sleep(5); // Simulate work

    // Shutdown sequence: metrics (wrapper) then perf which flushes & reports
    po_metrics_shutdown();
    po_perf_shutdown(stdout); // prints counters, timers, histograms
    return 0;
}
