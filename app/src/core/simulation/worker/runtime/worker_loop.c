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

static volatile sig_atomic_t g_worker_running = 0;

static void worker_handle_signal(int sig) {
	(void)sig;
	g_worker_running = 0;
}

/* Initialize runtime resources for worker (placeholder). */
int worker_init(void) {
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
	fprintf(stdout, "worker: entering run loop\n");

	while (g_worker_running) {
		/* Placeholder: poll for work, handle IPC, process tickets, etc. */
		sleep(1);
	}

	/* Graceful shutdown */
	fprintf(stdout, "worker: shutting down\n");
	/* TODO: cleanup IPC, flush metrics, release resources */
	return EXIT_SUCCESS;
}

