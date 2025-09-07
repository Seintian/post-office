// Director process: collects system info and (now) initializes performance
// instrumentation so that counters, timers and histograms accumulated by any
// library code used during its lifetime can be reported at shutdown.

#include <stdio.h>
#include <unistd.h>

#include <postoffice/sysinfo/sysinfo.h>
#include <postoffice/perf/perf.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/utils/errors.h>
#include <errno.h>

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

    po_sysinfo_t sysinfo;
    if (po_sysinfo_collect(&sysinfo) != 0) {
        fprintf(stderr, "Failed to collect system information\n");
        po_metrics_shutdown();
        po_perf_shutdown(stdout); // still emits what it has, if initialized
        return 1;
    }

    po_sysinfo_print(&sysinfo, stdout);

    // Shutdown sequence: metrics (wrapper) then perf which flushes & reports
    po_metrics_shutdown();
    po_perf_shutdown(stdout); // prints counters, timers, histograms
    return 0;
}
