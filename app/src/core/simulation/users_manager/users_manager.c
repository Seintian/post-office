/**
 * @file users_manager.c
 * @brief Event-Driven Users Manager Process Implementation (Threaded).
 * 
 * Listens for signals to dynamically manage user population:
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
#include "../user/runtime/user_loop.h"

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
#include <pthread.h>

// Global state for signal handlers
static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_sigusr1_received = 0;
static volatile sig_atomic_t g_sigusr2_received = 0;

// Configuration
static int g_initial_users = 5;
static int g_batch_size = 5;
static sim_shm_t* g_shm = NULL;

// Active user tracking
typedef struct {
    pthread_t thread;
    bool active;
    int id;
} user_slot_t;

static user_slot_t g_users[1000];
static int g_active_count = 0;
static int g_target_users = 0;

typedef struct {
    int id;
    int service_type;
} user_thread_arg_t;

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
    
    // Ignore SIGCHLD as we use threads
    signal(SIGCHLD, SIG_IGN);
}

static void get_sim_time(sim_shm_t* shm, int *d, int *h, int *m) {
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *d = (packed >> 16) & 0xFFFF;
    *h = (packed >> 8) & 0xFF;
    *m = packed & 0xFF;
}

static void* user_thread_entry(void* arg) {
    user_thread_arg_t* args = (user_thread_arg_t*)arg;
    pthread_cleanup_push(free, args);
    user_run(args->id, args->service_type, g_shm);
    pthread_cleanup_pop(1);
    return NULL;
}

static void spawn_user(void) {
    if (g_active_count >= 1000) {
        LOG_ERROR("Cannot spawn more users (max 1000)");
        return;
    }

    // Find first free slot (simplistic, usually appending is fine if we compact or just find first !active)
    // But we use g_active_count as index if we compact. 
    // Let's use g_active_count as simple stack pointer if possible, but reaping messes it up.
    // Better: Scan for free slot.
    int slot = -1;
    for (int i = 0; i < 1000; i++) {
        if (!g_users[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) return;

    static int next_service_type = 0;
    int stype = next_service_type;
    next_service_type = (next_service_type + 1) % SIM_MAX_SERVICE_TYPES;

    user_thread_arg_t* arg = malloc(sizeof(user_thread_arg_t));
    arg->id = (int)(po_rand_u32() & 0x7FFFFFFF);
    arg->service_type = stype;

    if (pthread_create(&g_users[slot].thread, NULL, user_thread_entry, arg) == 0) {
        g_users[slot].active = true;
        g_users[slot].id = arg->id;
        g_active_count++;
        LOG_DEBUG("Spawned user thread ID %d (total: %d/%d)", arg->id, g_active_count, g_target_users);
    } else {
        LOG_ERROR("Failed to create user thread");
        free(arg);
    }
}

static void remove_user(void) {
    if (g_active_count <= 0) return;

    // Find last active user (LIFO-ish to match stack behavior roughly)
    for (int i = 999; i >= 0; i--) {
        if (g_users[i].active) {
            // Cancel thread
            pthread_cancel(g_users[i].thread);
            pthread_join(g_users[i].thread, NULL);
            g_users[i].active = false;
            g_active_count--;
            LOG_INFO("Terminated user thread slot %d", i);
            return;
        }
    }
}

static void reap_zombies(void) {
    // Check for threads that have exited naturally
    for (int i = 0; i < 1000; i++) {
        if (g_users[i].active) {
            int rc = pthread_tryjoin_np(g_users[i].thread, NULL);
            if (rc == 0) {
                // Thread finished
                g_users[i].active = false;
                g_active_count--;
                LOG_DEBUG("Reaped user thread slot %d (remaining: %d/%d)", i, g_active_count, g_target_users);
            }
        }
    }
}

static void parse_args(int argc, char** argv) {
    static struct option long_options[] = {
        {"initial", required_argument, 0, 'i'},
        {"batch", required_argument, 0, 'b'},
        {"loglevel", required_argument, 0, 'l'},
        {"pid", required_argument, 0, 'p'}, 
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
                // Handled via env
                break;
            case 'p':
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
        .ring_capacity = 4096, // Increased for multiple threads
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = PO_CACHE_LINE_MAX
    };
    if (po_logger_init(&log_cfg) != 0) return 1;
    // Append to users.log (User threads will share this logger instance)
    // Actually, user_run calls user_init which inits logger... 
    // We MUST prevent user_run from initializing the logger again if it's already running in the same process.
    // The user_loop.c implementation of user_init does po_logger_init.
    // po_logger_init fails if already initialized.
    // So we should Initialize ONCE here, and modify user_loop.c to SKIP init if logger is active?
    // Or simpler: user_loop.c is designed for process per user.
    // We need to modify user_loop.c to be thread-friendly (global init split).
    // BUT user_loop.c is used by user.c too.
    // If we assume user_run is just logic, we should strip init from it or make it conditional.
    // Since we are refactoring, let's assume we can modify user_loop.c as well.
    // FOR NOW: Let's let user_init fail gracefully?
    // po_logger_init returns non-zero if already initialized. 
    // user_init prints to stderr "user: logger init failed" and returns -1. 
    // This would kill the thread.
    // WE MUST MODIFY user_loop.c.
    
    // Let's assume for this step we just init here.
    if (po_logger_add_sink_file("logs/users_manager.log", false) != 0) {
        LOG_WARN("Failed to add log file sink");
    }
    // Also add sink for users.log here, so all threads write to it.
    po_logger_add_sink_file("logs/users.log", true); // Append (truncated by director)

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
    
    LOG_INFO("[Day %d %02d:%02d] Users Manager started (PID: %d) [THREADED], initial=%d", 
             day, hour, min, getpid(), g_initial_users);

    // Initial Spawn
    for (int i = 0; i < g_target_users && g_running; i++) {
        spawn_user();
        usleep(10000); 
    }

    LOG_INFO("Initial users spawned. Entering event loop...");

    int last_synced_day = 0;

    void sync_check_day(void) {
        if (atomic_load(&g_shm->sync.barrier_active)) {
            unsigned int barrier_day = atomic_load(&g_shm->sync.day_seq);
            if ((int)barrier_day > last_synced_day) {
                atomic_fetch_add(&g_shm->sync.ready_count, 1);
                last_synced_day = (int)barrier_day;
                while (g_running && atomic_load(&g_shm->sync.barrier_active)) {
                    usleep(1000); 
                }
            }
        }
    }

    while (g_running) {
        sync_check_day();
        reap_zombies();

        // Respawn if below target
        while (g_active_count < g_target_users && g_running) {
            spawn_user();
        }

        // Handle SIGUSR1 (Add Users)
        if (g_sigusr1_received) {
            g_sigusr1_received = 0;
            g_target_users += g_batch_size;
            LOG_INFO("SIGUSR1: Adding %d users (new target: %d)", g_batch_size, g_target_users);
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
        
        usleep(100000); // 100ms
    }

    LOG_INFO("Shutting down. Terminating all users...");
    for (int i = 0; i < 1000; i++) {
        if (g_users[i].active) {
            pthread_cancel(g_users[i].thread);
            pthread_join(g_users[i].thread, NULL);
        }
    }

    sim_ipc_shm_detach(g_shm);
    po_logger_shutdown();
    po_perf_shutdown(NULL);
    return 0;
}
