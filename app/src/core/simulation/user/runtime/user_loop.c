/* Simple user runtime loop skeleton.
 * Provides a minimal lifecycle for simulated user processes. Real behavior
 * (request generation, ticket handling) should be added where noted.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

static volatile sig_atomic_t g_user_running = 0;

static void user_handle_signal(int sig) {
	(void)sig;
	g_user_running = 0;
}

int user_init(void) {
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
	fprintf(stdout, "user: entering run loop\n");

	while (g_user_running) {
		/* Placeholder: generate probabilistic requests, await ticket responses */
		sleep(1);
	}

	fprintf(stdout, "user: shutting down\n");
	/* TODO: cleanup resources */
	return EXIT_SUCCESS;
}

