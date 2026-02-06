#define _POSIX_C_SOURCE 200809L
#include "director_orch.h"

#include <errno.h>
#include <postoffice/log/logger.h>
#include <postoffice/vector/vector.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

static po_vector_t *g_pids = NULL;

void initialize_process_orchestrator(void) {
    g_pids = po_vector_create();
}

static void launch_process(const char *bin, char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        execv(bin, argv);
        _exit(1);
    } else if (pid > 0) {
        LOG_INFO("Launched %s (PID %d)", bin, pid);
        if (g_pids)
            po_vector_push(g_pids, (void *)(intptr_t)pid);
    }
}

void spawn_simulation_subsystems(const director_config_t *cfg) {
    // A. Ticket Issuer
    char pool_str[16];
    snprintf(pool_str, sizeof(pool_str), "%d", cfg->issuer_pool_size);
    char *args_ti[] = {"bin/post_office_ticket_issuer",
                       "-l",
                       (char *)cfg->log_level,
                       "--pool-size",
                       pool_str,
                       NULL};
    launch_process("bin/post_office_ticket_issuer", args_ti);

    // B. Workers
    char workers_str[16];
    snprintf(workers_str, sizeof(workers_str), "%u", cfg->worker_count);
    char *args_w[] = {
        "bin/post_office_worker", "-l", (char *)cfg->log_level, "-w", workers_str, NULL};
    launch_process("bin/post_office_worker", args_w);

    // C. Users Manager
    char init_str[16], batch_str[16], um_pool_str[16];
    snprintf(init_str, sizeof(init_str), "%d", cfg->initial_users);
    snprintf(batch_str, sizeof(batch_str), "%d", cfg->batch_users);
    snprintf(um_pool_str, sizeof(um_pool_str), "%d", cfg->manager_pool_size);
    char *args_um[] = {"bin/post_office_users_manager",
                       "-l",
                       (char *)cfg->log_level,
                       "--initial",
                       init_str,
                       "--batch",
                       batch_str,
                       "--pool-size",
                       um_pool_str,
                       NULL};
    launch_process("bin/post_office_users_manager", args_um);
}

void terminate_all_simulation_subsystems(void) {
    if (!g_pids)
        return;

    size_t count = po_vector_size(g_pids);
    for (size_t i = 0; i < count; i++) {
        pid_t p = (pid_t)(intptr_t)po_vector_at(g_pids, i);
        if (p > 0)
            kill(p, SIGTERM);
    }
    for (size_t i = 0; i < count; i++) {
        pid_t p = (pid_t)(intptr_t)po_vector_at(g_pids, i);
        if (p > 0)
            waitpid(p, NULL, 0);
    }
    po_vector_destroy(g_pids);
    g_pids = NULL;
}

bool director_orch_check_crashes(void) {
    if (!g_pids)
        return false;

    int status;
    pid_t pid;
    bool crash_detected = false;

    // Check all exited children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Attempt to find PID in our list
        ssize_t idx = -1;
        size_t count = po_vector_size(g_pids);
        for (size_t i = 0; i < count; i++) {
            if ((pid_t)(intptr_t)po_vector_at(g_pids, i) == pid) {
                idx = (ssize_t)i;
                break;
            }
        }

        if (idx != -1) {
            // Remove from vector so we don't double count or try to kill it later
            po_vector_remove(g_pids, (size_t)idx);

            if (WIFEXITED(status)) {
                int code = WEXITSTATUS(status);
                if (code != 0) {
                    LOG_ERROR("Process %d exited with error code %d", pid, code);
                    crash_detected = true;
                } else {
                    LOG_INFO("Process %d exited normally.", pid);
                    crash_detected = true;
                }
            } else if (WIFSIGNALED(status)) {
                int sig = WTERMSIG(status);
                LOG_ERROR("Process %d crashed by signal %d (%s)", pid, sig, strsignal(sig));
                crash_detected = true;
            }
        } else {
            // Unknown child?
            LOG_WARN("Unknown child process %d reaped.", pid);
        }
    }

    return crash_detected;
}
