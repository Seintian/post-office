/* Simple worker runtime loop skeleton.
 * This file provides a minimal lifecycle for worker processes: initialization,
 * a run loop that exits on signal, and a graceful shutdown sequence. Fill
 * in worker-specific behavior (IPC, task polls, metrics) later.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "worker_loop.h"
#include <postoffice/log/logger.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>
#include <postoffice/sysinfo/sysinfo.h>

static volatile sig_atomic_t g_worker_running = 0;

static void worker_handle_signal(int sig) {
	(void)sig;
	g_worker_running = 0;
}

/* Initialize runtime resources for worker (placeholder). */
int worker_init(void) {
    // Collect system information for optimizations
    po_sysinfo_t sysinfo;
    size_t cacheline_size = 64; // default
    if (po_sysinfo_collect(&sysinfo) == 0 && sysinfo.dcache_lnsize > 0) {
        cacheline_size = (size_t)sysinfo.dcache_lnsize;
    }

    if (po_metrics_init() != 0) {
        fprintf(stderr, "worker: metrics init failed\n");
    }

    po_logger_config_t log_cfg = {
        .level = LOG_INFO,
        .ring_capacity = 4096,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = cacheline_size
    };
    if (po_logger_init(&log_cfg) != 0) {
        fprintf(stderr, "worker: logger init failed\n");
        return -1;
    }
    po_logger_add_sink_console(true);

	/* TODO: init IPC, state, metrics, etc. */
	return 0;
}

/* Main run loop for worker. Blocks until termination requested. */
int worker_run(void) {
	struct sigaction sa;
	sa.sa_handler = worker_handle_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	if (worker_init() != 0) {
		fprintf(stderr, "worker: initialization failed\n");
		return EXIT_FAILURE;
	}

	g_worker_running = 1;
	LOG_INFO("worker: entering run loop");

	while (g_worker_running) {
		/* Placeholder: poll for work, handle IPC, process tickets, etc. */
		sleep(1);
	}

	/* Graceful shutdown */
	LOG_INFO("worker: shutting down");
    po_metrics_shutdown();
    po_perf_shutdown(stdout);
    po_logger_shutdown();
	return EXIT_SUCCESS;
}
