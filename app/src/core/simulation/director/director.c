// Director process: collects system info and (now) initializes performance
// instrumentation so that counters, timers and histograms accumulated by any
// library code used during its lifetime can be reported at shutdown.

#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>
#include <utils/errors.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "ctrl_bridge/bridge_mainloop.h"

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

    /* Start control bridge so external clients (users, UI) may send
     * commands pre/during/post simulation. The bridge runs in a
     * background thread and is intentionally minimal; command handling
     * should be wired to Director APIs in future work.
     */
    pthread_t bridge_thread;
    int bridge_started = 0;
    if (bridge_mainloop_init() == 0) {
        /* Start bridge in a dedicated thread. Use a small wrapper to adapt
         * the signature to pthread_create.
         */
        void* bridge_thread_fn(void* _) {
            (void) _;
            bridge_mainloop_run();
            return NULL;
        }

        if (pthread_create(&bridge_thread, NULL, bridge_thread_fn, NULL) != 0) {
            fprintf(stderr, "director: failed to start bridge thread\n");
        } else {
            bridge_started = 1;
        }
    } else {
        fprintf(stderr, "director: bridge init failed; continuing without control bridge\n");
    }

    /* Director main loop would go here (or sleep for now) */
    sleep(5); // Simulate work

    /* Stop control bridge, wait for thread to exit if it was started. */
    bridge_mainloop_stop();
    /* Join the bridge thread if it was started. */
    if (bridge_started) {
        pthread_join(bridge_thread, NULL);
    }

    /* Shutdown sequence: metrics (wrapper) then perf which flushes & reports */
    po_metrics_shutdown();
    po_perf_shutdown(stdout); // prints counters, timers, histograms
    return 0;
}
