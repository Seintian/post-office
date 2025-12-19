#define _POSIX_C_SOURCE 200809L

#include <getopt.h>
#include <postoffice/concurrency/threadpool.h>
#include <postoffice/log/logger.h>
#include <postoffice/perf/cache.h>
#include <postoffice/perf/perf.h>
#include <postoffice/random/random.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utils/errors.h>
#include <utils/signals.h>

#include "../ipc/simulation_ipc.h"
#include "../ipc/simulation_protocol.h"
#include "../user/runtime/user_loop.h"

// Global state for signal handlers
static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_sigusr1_received = 0;
static volatile sig_atomic_t g_sigusr2_received = 0;

// Configuration
static int g_initial_users = 5;
static int g_batch_size = 5;
static sim_shm_t *g_shm = NULL;
static threadpool_t *g_pool = NULL;

// Active user tracking
typedef struct {
    volatile atomic_bool active;     // Slot allocation status
    volatile atomic_bool should_run; // Cancellation flag passed to user_run
    int id;
} user_slot_t;

static user_slot_t g_users[1000];
static atomic_int g_active_count = 0;
static int g_target_users = 0;

typedef struct {
    int id;
    int service_type;
    int slot_idx;
} user_task_arg_t;

static void handle_sigusr1(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)info;
    (void)context;
    g_sigusr1_received = 1;
}

static void handle_sigusr2(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)info;
    (void)context;
    g_sigusr2_received = 1;
}

static void handle_terminate(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)info;
    (void)context;
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

static void get_sim_time(sim_shm_t *shm, int *d, int *h, int *m) {
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *d = (packed >> 16) & 0xFFFF;
    *h = (packed >> 8) & 0xFF;
    *m = packed & 0xFF;
}

static void user_task_wrapper(void *arg) {
    user_task_arg_t *args = (user_task_arg_t *)arg;
    int slot = args->slot_idx;

    user_run(args->id, args->service_type, g_shm, &g_users[slot].should_run);

    // Cleanup
    atomic_store(&g_users[slot].active, false);
    atomic_fetch_sub(&g_active_count, 1);

    int day, hour, min;
    get_sim_time(g_shm, &day, &hour, &min);
    // LOG_DEBUG("[Day %d %02d:%02d] User task %d (slot %d) finished", day, hour, min, args->id,
    // slot); // Too noisy?

    free(args);
}

static void spawn_user(void) {
    if (atomic_load(&g_active_count) >= 1000) {
        LOG_ERROR("Cannot spawn more users (max 1000)");
        return;
    }

    // Find first free slot
    int slot = -1;
    for (int i = 0; i < 1000; i++) {
        // Atomic CAS to claim slot
        bool expected = false;
        if (atomic_compare_exchange_strong(&g_users[i].active, &expected, true)) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        LOG_DEBUG("No free slot found for new user");
        return;
    }

    static int next_service_type = 0;
    int stype = next_service_type;
    next_service_type = (next_service_type + 1) % SIM_MAX_SERVICE_TYPES;

    user_task_arg_t *arg = malloc(sizeof(user_task_arg_t));
    if (!arg) {
        atomic_store(&g_users[slot].active, false);
        return;
    }

    arg->id = (int)(po_rand_u32() & 0x7FFFFFFF);
    arg->service_type = stype;
    arg->slot_idx = slot;

    atomic_store(&g_users[slot].should_run, true);
    g_users[slot].id = arg->id;
    int user_id = arg->id;

    if (tp_submit(g_pool, user_task_wrapper, arg) == 0) {
        atomic_fetch_add(&g_active_count, 1);
        LOG_DEBUG("Spawned user %d (slot %d) (total: %d/%d)", user_id, slot,
                  atomic_load(&g_active_count), g_target_users);
    } else {
        LOG_ERROR("Failed to submit user task");
        atomic_store(&g_users[slot].active, false);
        free(arg);
    }
}

static void remove_user(void) {
    if (atomic_load(&g_active_count) <= 0)
        return;

    // Signal random or last user to stop.
    // LIFO for consistency with old behavior
    for (int i = 999; i >= 0; i--) {
        if (atomic_load(&g_users[i].active) && atomic_load(&g_users[i].should_run)) {
            atomic_store(&g_users[i].should_run, false);
            // Decrease count logically here? No, let wrapper do it.
            // But we can verify later.
            LOG_INFO("Signaled user slot %d to stop", i);
            return;
        }
    }
}

static size_t g_pool_size = 1000;

static void parse_args(int argc, char **argv) {
    static struct option long_options[] = {
        {"initial", required_argument, 0, 'i'},   {"batch", required_argument, 0, 'b'},
        {"loglevel", required_argument, 0, 'l'},  {"pid", required_argument, 0, 'p'},
        {"pool-size", required_argument, 0, 's'}, {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "i:b:l:p:s:", long_options, NULL)) != -1) {
        switch (opt) {
        case 'i': {
            char *endptr;
            unsigned long val = strtoul(optarg, &endptr, 10);
            if (*endptr == '\0' && val > 0 && val <= 1000) {
                g_initial_users = (int)val;
            }
            break;
        }
        case 'b': {
            char *endptr;
            unsigned long val = strtoul(optarg, &endptr, 10);
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
        case 's': {
            char *endptr;
            unsigned long val = strtoul(optarg, &endptr, 10);
            if (*endptr == '\0' && val > 1) { // Min 2
                g_pool_size = val;
            }
            break;
        }
        }
    }
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

    // ... (logger) ...
    // 1. Init Logger
    int level = LOG_INFO;
    char *env = getenv("PO_LOG_LEVEL");
    if (env) {
        int l = po_logger_level_from_str(env);
        if (l != -1)
            level = l;
    }

    po_logger_config_t log_cfg = {.level = level,
                                  .ring_capacity = 4096,
                                  .consumers = 1,
                                  .policy = LOGGER_OVERWRITE_OLDEST,
                                  .cacheline_bytes = PO_CACHE_LINE_MAX};
    if (po_logger_init(&log_cfg) != 0)
        return 1;

    if (po_logger_add_sink_file("logs/users_manager.log", false) != 0) {
        LOG_WARN("Failed to add log file sink");
    }
    po_logger_add_sink_file("logs/users.log", true);

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

    // Initialize Thread Pool
    // Size = 1000 to allow all potential users to run concurrently
    g_pool = tp_create(g_pool_size, 2000);
    if (!g_pool) {
        LOG_FATAL("Failed to create thread pool");
        sim_ipc_shm_detach(g_shm);
        po_logger_shutdown();
        return 1;
    }

    g_target_users = g_initial_users;

    LOG_INFO(
        "[Day %d %02d:%02d] Users Manager started (PID: %d) [THREADPOOL], initial=%d, pool=%zu",
        day, hour, min, getpid(), g_initial_users, g_pool_size);

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
                
                // Signal readiness
                pthread_mutex_lock(&g_shm->sync.mutex);
                pthread_cond_signal(&g_shm->sync.cond_workers_ready);

                struct timespec ts;
                while (g_running && atomic_load(&g_shm->sync.barrier_active) &&
                       atomic_load(&g_shm->sync.day_seq) == barrier_day) {
                    clock_gettime(CLOCK_MONOTONIC, &ts);
                    ts.tv_sec += 1;
                    pthread_cond_timedwait(&g_shm->sync.cond_day_start, &g_shm->sync.mutex, &ts);
                }
                pthread_mutex_unlock(&g_shm->sync.mutex);
            }
        }
    }

    while (g_running) {
        sync_check_day();
        // Zombies are auto-reaped by wrapper now, but we verify active count

        // Respawn if below target (and verify we aren't saturating spawn attempts)
        while (atomic_load(&g_active_count) < g_target_users && g_running) {
            spawn_user();
            usleep(1000); // Slight throttle
        }

        // Handle SIGUSR1 (Add Users)
        if (g_sigusr1_received) {
            g_sigusr1_received = 0;
            g_target_users += g_batch_size;
            if (g_target_users > 1000)
                g_target_users = 1000;
            LOG_INFO("SIGUSR1: Adding %d users (new target: %d)", g_batch_size, g_target_users);
        }

        // Handle SIGUSR2 (Remove Users)
        if (g_sigusr2_received) {
            g_sigusr2_received = 0;
            int to_remove = g_batch_size;
            if (g_target_users - to_remove < 0)
                to_remove = g_target_users;

            g_target_users -= to_remove;
            LOG_INFO("SIGUSR2: Removing %d users (new target: %d)", to_remove, g_target_users);

            for (int i = 0; i < to_remove; i++) {
                remove_user();
            }
        }

        usleep(100000); // 100ms
    }

    LOG_INFO("Shutting down. Terminating all users...");
    // Signal all to stop
    for (int i = 0; i < 1000; i++) {
        if (atomic_load(&g_users[i].active)) {
            atomic_store(&g_users[i].should_run, false);
        }
    }

    tp_destroy(g_pool, true);

    sim_ipc_shm_detach(g_shm);
    po_logger_shutdown();
    po_perf_shutdown(NULL);
    return 0;
}
