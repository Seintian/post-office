/* Simple user runtime loop skeleton.
 * Provides a minimal lifecycle for simulated user processes. Real behavior
 * (request generation, ticket handling) should be added where noted.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "user_loop.h"
#include <postoffice/log/logger.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>

static volatile sig_atomic_t g_user_running = 0;

static void user_handle_signal(int sig) {
	(void)sig;
	g_user_running = 0;
}

int user_init(void) {
    if (po_perf_init(16, 8, 4) != 0) {
        fprintf(stderr, "user: perf init failed\n");
    }
    if (po_metrics_init() != 0) {
        fprintf(stderr, "user: metrics init failed\n");
    }

    po_logger_config_t log_cfg = {
        .level = LOG_INFO,
        .ring_capacity = 4096,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST
    };
    if (po_logger_init(&log_cfg) != 0) {
        fprintf(stderr, "user: logger init failed\n");
        return -1;
    }
    po_logger_add_sink_console(true);

	/* TODO: initialize user IPC, random seed, local state */
	return 0;
}

int user_run(void) {
	struct sigaction sa;
	sa.sa_handler = user_handle_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	if (user_init() != 0) {
		fprintf(stderr, "user: initialization failed\n");
		return EXIT_FAILURE;
	}

	g_user_running = 1;
	LOG_INFO("user: entering run loop");

	while (g_user_running) {
		/* Placeholder: generate probabilistic requests, await ticket responses */
		sleep(1);
	}

	LOG_INFO("user: shutting down");
    po_metrics_shutdown();
    po_perf_shutdown(stdout);
    po_logger_shutdown();
	return EXIT_SUCCESS;
}
