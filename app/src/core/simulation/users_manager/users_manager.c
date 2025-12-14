/**
 * @file users_manager.c
 * @brief Users Manager Process Implementation.
 */
#define _POSIX_C_SOURCE 200809L

#include <postoffice/log/logger.h>
#include <utils/errors.h>
#include "../ipc/simulation_protocol.h"
#include "../ipc/simulation_ipc.h"

#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <utils/signals.h>

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig, siginfo_t* info, void* context) {
    (void)sig; (void)info; (void)context;
    g_running = 0;
}

static void setup_signals(void) {
    if (sigutil_handle_terminating(handle_signal, 0) != 0) {
         fprintf(stderr, "Failed to register signal handlers\n");
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    // Expected args: --loglevel <lvl> --pid <director_pid>
    // but we might want to know how many users to spawn.
    // For now, hardcode or use env var.
    
    int n_users = 10;
    char *env_n = getenv("PO_USERS_COUNT");
    if (env_n) n_users = atoi(env_n);

    // 1. Init Logger
    po_logger_config_t log_cfg = {
        .level = LOG_INFO,
        .ring_capacity = 1024,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = 64
    };
    if (po_logger_init(&log_cfg) != 0) return 1;
    po_logger_add_sink_file("logs/users_manager.log", false);

    // Attach SHM for time
    sim_shm_t* shm = sim_ipc_shm_attach();
    if (!shm) {
        LOG_ERROR("users_manager: failed to attach SHM");
        po_logger_shutdown();
        return 1;
    }
    sim_time_t *t = &shm->time_control;

    setup_signals();
    LOG_INFO("[Day %d %02d:%02d] Users Manager started (PID: %d), spawning %d users", 
             atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute),
             getpid(), n_users);

    // 2. Spawn Users
    const char *user_bin = "bin/post_office_user";
    
    // Check if bin exists
    if (access(user_bin, X_OK) != 0) {
        LOG_WARN("Cannot find user bin at '%s', trying './post_office_user'", user_bin);
        user_bin = "./post_office_user"; // Fallback if CWD is bin/
    }

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    for (int i = 0; i < n_users && g_running; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child
            char id_str[16];
            char param_str[16];
            snprintf(id_str, sizeof(id_str), "%d", i + 1);
            snprintf(param_str, sizeof(param_str), "%d", rand() % SIM_MAX_SERVICE_TYPES);

            // execl only returns on error
            execl(user_bin, "post_office_user", "-i", id_str, "-s", param_str, NULL);
            
            // If failed
            LOG_ERROR("Failed to exec user: %s", strerror(errno));
            exit(1);
        } else if (pid > 0) {
            // Parent
            usleep(10000); // Stagger spawns slightly
        } else {
            LOG_ERROR("Fork failed");
        }
    }

    LOG_INFO("[Day %d %02d:%02d] All initial users spawned. Watching...", 
             atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute));

    // 3. Wait loop
    while (g_running) {
        int status;
        pid_t wpid = waitpid(-1, &status, WNOHANG);
        if (wpid > 0) {
            // User exited
            // LOG_INFO("User PID %d exited", wpid);
        } else if (wpid == 0) {
            sleep(1);
        } else {
            if (errno == ECHILD) break; // No more children
        }
    }

    po_logger_shutdown();
    return 0;
}
