#define _POSIX_C_SOURCE 200809L
#include "simulation_lifecycle.h"
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <spawn.h>
#include <string.h>

extern char **environ;

const char* g_simulation_config_path = NULL;
static pid_t g_director_pid = -1;
static volatile sig_atomic_t g_running = 0;

void simulation_init(const char* config_path) {
    g_simulation_config_path = config_path; // Assumes config_path lifetime is managed by caller (main args)
}

void simulation_start(bool tui_mode) {
    if (g_director_pid > 0) return; // Already running

    printf("Starting simulation (Director)...\n");

    // Path to director executable. Assuming strictly 'bin/post_office_director'
    // This assumes the CWD is the project root, which matches 'make start' behavior.
    const char* exe_path = "bin/post_office_director";

    if (access(exe_path, X_OK) != 0) {
        fprintf(stderr, "Error: Cannot find director executable at '%s'. Make sure to build it first.\n", exe_path);
        return;
    }

    // Build args
    char* argv[10];
    int argc = 0;
    argv[argc++] = (char*)exe_path;
    if (g_simulation_config_path) {
        argv[argc++] = "--config";
        argv[argc++] = (char*)g_simulation_config_path;
    }
    argv[argc++] = NULL;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    // Default: inherit stdin, stdout, stderr
    if (tui_mode) {
        // Redirect stdout to /dev/null to prevent TUI corruption
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0644);
    }

    int status = posix_spawn(&g_director_pid, exe_path, &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);

    if (status != 0) {
        fprintf(stderr, "Error: posix_spawn failed: %s\n", strerror(status));
        g_director_pid = -1;
    } else {
        printf("Director started (PID: %d)\n", g_director_pid);
    }
}

void simulation_stop(void) {
    if (g_director_pid > 0) {
        printf("Stopping Director (PID: %d)...\n", g_director_pid);
        kill(g_director_pid, SIGTERM);

        int status;
        waitpid(g_director_pid, &status, 0);

        g_director_pid = -1;
        printf("Director stopped.\n");
    }
}

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

void simulation_run_headless(void) {
    simulation_start(false);
    if (g_director_pid <= 0) return;

    g_running = 1;

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Trap SIGINT and SIGTERM to clean up child process
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("Running in headless mode. Press Ctrl+C to stop.\n");

    while (g_running) {
        // Check if director is still alive
        int status;
        pid_t result = waitpid(g_director_pid, &status, WNOHANG);
        if (result == g_director_pid) {
            printf("Director exited unexpectedly.\n");
            g_director_pid = -1;
            break;
        } else if (result < 0) {
            // Error or no child
            break;
        }

        // Sleep to save CPU
        sleep(1); 
    }

    simulation_stop();
}
