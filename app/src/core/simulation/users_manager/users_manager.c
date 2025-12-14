/**
 * @file users_manager.c
 * @brief Event-Driven Users Manager Process Implementation.
 * 
 * Listens for signals to dynamically manage user population:
 * - SIGCHLD: Detects user exit, respawns immediately
 * - SIGUSR1: Adds batch_size users to population
 * - SIGUSR2: Removes batch_size users from population
 */
#define _POSIX_C_SOURCE 200809L

#include <postoffice/log/logger.h>
#include <utils/errors.h>
#include <postoffice/perf/cache.h>
#include <postoffice/perf/perf.h>
#include "../ipc/simulation_protocol.h"
#include <postoffice/random/random.h>
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
#include <getopt.h>

// Global state for signal handlers
static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_sigchld_received = 0;
static volatile sig_atomic_t g_sigusr1_received = 0;
static volatile sig_atomic_t g_sigusr2_received = 0;

// Configuration
static int g_initial_users = 5;
static int g_batch_size = 5;
static const char* g_log_level_str = "INFO";
static sim_shm_t* g_shm = NULL;

// Active user tracking
static pid_t g_user_pids[1000]; // Track PIDs
static int g_active_users = 0;
static int g_target_users = 0;

static void handle_sigchld(int sig, siginfo_t* info, void* context) {
    (void)sig; (void)info; (void)context;
    g_sigchld_received = 1;
}

static void handle_sigusr1(int sig, siginfo_t* info, void* context) {
    (void)sig; (void)info; (void)context;
    g_sigusr1_received = 1;
}

static void handle_sigusr2(int sig, siginfo_t* info, void* context) {
    (void)sig; (void)info; (void)context;
    g_sigusr2_received = 1;
}

static void handle_terminate(int sig, siginfo_t* info, void* context) {
    (void)sig; (void)info; (void)context;
    g_running = 0;
}

static void setup_signals(void) {
    struct sigaction sa;

    // SIGCHLD
    sa.sa_sigaction = handle_sigchld;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);
    
    // SIGUSR1
    sa.sa_sigaction = handle_sigusr1;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    // SIGUSR2
    sa.sa_sigaction = handle_sigusr2;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR2, &sa, NULL);

    // Termination signals
    sa.sa_sigaction = handle_terminate;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

static void get_sim_time(sim_shm_t* shm, int *d, int *h, int *m) {
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *d = (packed >> 16) & 0xFFFF;
    *h = (packed >> 8) & 0xFF;
    *m = packed & 0xFF;
}

static void spawn_user(void) {
    if (g_active_users >= 1000) {
        LOG_ERROR("Cannot spawn more users (max 1000)");
        return;
    }

    const char *user_bin = "bin/post_office_user";
    pid_t pid = fork();

    if (pid == 0) {
        // Child
        char id_str[16];
        char param_str[16];
        snprintf(id_str, sizeof(id_str), "%d", (int)(po_rand_u32() & 0x7FFFFFFF)); 
        snprintf(param_str, sizeof(param_str), "%u", po_rand_u32() % SIM_MAX_SERVICE_TYPES);

        execl(user_bin, "post_office_user", "-l", g_log_level_str, "-i", id_str, "-s", param_str, NULL);

        const char *msg = "Failed to exec user\n";
        if (write(STDERR_FILENO, msg, strlen(msg)) == -1) {}
        _exit(1);
    } else if (pid > 0) {
        // Parent
        g_user_pids[g_active_users++] = pid;
        LOG_DEBUG("Spawned user PID %d (total: %d/%d)", pid, g_active_users, g_target_users);
    } else {
        LOG_ERROR("Fork failed: %s", strerror(errno));
    }
}

static void remove_user(void) {
    if (g_active_users <= 0) return;

    // Kill the most recent user
    pid_t pid = g_user_pids[g_active_users - 1];
    if (kill(pid, SIGTERM) == 0) {
        LOG_INFO("Terminated user PID %d", pid);
        g_active_users--;
    } else {
        LOG_WARN("Failed to kill PID %d: %s", pid, strerror(errno));
    }
}

static void reap_zombies(void) {
    int status;
    pid_t wpid;

    while ((wpid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Find and remove from array
        for (int i = 0; i < g_active_users; i++) {
            if (g_user_pids[i] == wpid) {
                // Shift array
                for (int j = i; j < g_active_users - 1; j++) {
                    g_user_pids[j] = g_user_pids[j + 1];
                }
                g_active_users--;
                LOG_DEBUG("Reaped user PID %d (remaining: %d/%d)", wpid, g_active_users, g_target_users);
                break;
            }
        }
    }
}

static void parse_args(int argc, char** argv) {
    static struct option long_options[] = {
        {"initial", required_argument, 0, 'i'},
        {"batch", required_argument, 0, 'b'},
        {"loglevel", required_argument, 0, 'l'},
        {"pid", required_argument, 0, 'p'}, // Director PID (for compatibility)
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:b:l:p:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i': {
                char *endptr;
                long val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val > 0 && val <= 1000) {
                    g_initial_users = (int)val;
                }
                break;
            }
            case 'b': {
                char *endptr;
                long val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val > 0 && val <= 100) {
                    g_batch_size = (int)val;
                }
                break;
            }
            case 'l':
                g_log_level_str = optarg;
                break;
            case 'p':
                // Ignored (kept for compatibility)
                break;
        }
    }
}

int main(int argc, char** argv) {
    parse_args(argc, argv);

    // 1. Init Logger
    int level = LOG_INFO;
    char *env = getenv("PO_LOG_LEVEL");
    if (env) {
        int l = po_logger_level_from_str(env);
        if (l != -1) level = l;
    }

    po_logger_config_t log_cfg = {
        .level = level,
        .ring_capacity = 1024,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = PO_CACHE_LINE_MAX
    };
    if (po_logger_init(&log_cfg) != 0) return 1;
    if (po_logger_add_sink_file("logs/users_manager.log", false) != 0) {
        LOG_WARN("Failed to add log file sink");
    }

    // Attach SHM for time
    g_shm = sim_ipc_shm_attach();
    if (!g_shm) {
        LOG_ERROR("users_manager: failed to attach SHM");
        po_logger_shutdown();
        return 1;
    }

    int day, hour, min;
    get_sim_time(g_shm, &day, &hour, &min);

    setup_signals();
    po_rand_seed_auto();

    g_target_users = g_initial_users;
    
    LOG_INFO("[Day %d %02d:%02d] Users Manager started (PID: %d), initial=%d, batch=%d", 
             day, hour, min, getpid(), g_initial_users, g_batch_size);

    // Check if bin exists
    if (access("bin/post_office_user", X_OK) != 0) {
        LOG_ERROR("Cannot find user binary at 'bin/post_office_user'");
        sim_ipc_shm_detach(g_shm);
        po_logger_shutdown();
        po_perf_shutdown(NULL);
        return 1;
    }

    // Initial Spawn
    for (int i = 0; i < g_target_users && g_running; i++) {
        spawn_user();
        usleep(10000); // Brief delay
    }

    LOG_INFO("Initial users spawned. Entering event loop...");

    // Event-Driven Main Loop
    while (g_running) {
        // Wait for signals
        pause();

        // Handle SIGCHLD
        if (g_sigchld_received) {
            g_sigchld_received = 0;
            reap_zombies();

            // Respawn if below target
            while (g_active_users < g_target_users && g_running) {
                spawn_user();
            }
        }

        // Handle SIGUSR1 (Add Users)
        if (g_sigusr1_received) {
            g_sigusr1_received = 0;
            g_target_users += g_batch_size;
            LOG_INFO("SIGUSR1: Adding %d users (new target: %d)", g_batch_size, g_target_users);

            for (int i = 0; i < g_batch_size && g_running; i++) {
                spawn_user();
            }
        }

        // Handle SIGUSR2 (Remove Users)
        if (g_sigusr2_received) {
            g_sigusr2_received = 0;
            int to_remove = g_batch_size;
            if (g_target_users - to_remove < 0) to_remove = g_target_users;

            g_target_users -= to_remove;
            LOG_INFO("SIGUSR2: Removing %d users (new target: %d)", to_remove, g_target_users);

            for (int i = 0; i < to_remove; i++) {
                remove_user();
            }
        }
    }

    // Graceful Shutdown
    LOG_INFO("Shutting down. Terminating all users...");
    for (int i = 0; i < g_active_users; i++) {
        kill(g_user_pids[i], SIGTERM);
    }

    // Wait for remaining children
    while (waitpid(-1, NULL, 0) > 0) {}

    sim_ipc_shm_detach(g_shm);
    po_logger_shutdown();
    po_perf_shutdown(NULL);
    return 0;
}
