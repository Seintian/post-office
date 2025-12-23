#define _POSIX_C_SOURCE 200809L

#include "worker_loop.h"

#include <errno.h>
#include <postoffice/log/logger.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/cache.h>
#include <postoffice/perf/perf.h>
#include <postoffice/random/random.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <time.h>
#include <unistd.h>
#include <utils/errors.h>
#include <utils/signals.h>

#include "../../ipc/simulation_ipc.h"
#include "../../ipc/simulation_protocol.h"

static volatile sig_atomic_t g_running = 1;

/**
 * @brief Signal handler for graceful shutdown.
 * @param[in] sig Signal number.
 * @param[in] info Signal info.
 * @param[in] context User context.
 */
static void handle_signal(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)info;
    (void)context;
    g_running = 0;
}

/**
 * @brief Register terminating signal handlers.
 */
static void setup_signals(void) {
    if (sigutil_handle_terminating(handle_signal, 0) != 0) {
        fprintf(stderr, "Failed to register signal handlers\n");
    }
}

/**
 * @brief Extract sim time from SHM.
 * @param[in] shm Shared memory pointer.
 * @param[out] d Day.
 * @param[out] h Hour.
 * @param[out] m Minute.
 */
static void get_sim_time(sim_shm_t *shm, int *d, int *h, int *m) {
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *d = (packed >> 16) & 0xFFFF;
    *h = (packed >> 8) & 0xFF;
    *m = packed & 0xFF;
}

static sim_shm_t *g_shm_worker = NULL;
static int g_semid_worker = -1;

// Global initialization (called once by main thread)
/**
 * @brief Initialize worker global resources (SHM, Semaphores, Logger).
 * @return 0 on success, -1 on failure.
 */
int worker_global_init(void) {
    // Collect system information for optimizations
    po_sysinfo_t sysinfo;
    size_t cacheline_size = 64; // default
    if (po_sysinfo_collect(&sysinfo) == 0 && sysinfo.dcache_lnsize > 0) {
        cacheline_size = (size_t)sysinfo.dcache_lnsize;
    }

    // Start system info sampler for runtime adaptation
    po_sysinfo_sampler_init();

    if (po_metrics_init(0, 0, 0) != 0) {
        fprintf(stderr, "worker: metrics init failed\n");
    }

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
                                  .cacheline_bytes = cacheline_size};
    if (po_logger_init(&log_cfg) != 0) {
        fprintf(stderr, "worker: logger init failed\n");
        return -1;
    }
    po_logger_add_sink_console(true); // Keep console
    // Use shared logfile in append mode
    if (po_logger_add_sink_file("logs/workers.log", true) != 0) {
        LOG_WARN("Failed to add log file sink");
    }

    // Attach SHM
    g_shm_worker = sim_ipc_shm_attach();
    if (!g_shm_worker) {
        LOG_FATAL("worker: failed to attach SHM");
        return -1;
    }

    // Get Semaphores
    g_semid_worker = sim_ipc_sem_get();
    if (g_semid_worker == -1) {
        LOG_ERROR("worker: failed to get semaphores");
        return -1;
    }

    setup_signals();
    po_rand_seed_auto();

    return 0;
}

/**
 * @brief Cleanup worker global resources.
 */
void worker_global_cleanup(void) {
    if (g_shm_worker) {
        sim_ipc_shm_detach(g_shm_worker);
        g_shm_worker = NULL;
    }
    po_logger_shutdown();
}

// Helper for sync
/**
 * @brief Check and wait for daily synchronization barrier.
 * @param[in] shm Shared memory pointer.
 * @param[in,out] last_synced_day Tracker for last synced day.
 */
static void sync_check_day(sim_shm_t *shm, int *last_synced_day) {
    // Optimistic check (check without lock first)
    if (!atomic_load(&shm->sync.barrier_active))
        return;

    // Lock and re-check
    pthread_mutex_lock(&shm->sync.mutex);

    // Check if barrier is active AND we are in a new day
    unsigned int barrier_day = atomic_load(&shm->sync.day_seq);
    if (atomic_load(&shm->sync.barrier_active) && (int)barrier_day > *last_synced_day) {

        // Mark myself as ready
        atomic_fetch_add(&shm->sync.ready_count, 1);
        *last_synced_day = (int)barrier_day;

        // Wake director
        pthread_cond_signal(&shm->sync.cond_workers_ready);

        // Wait for release
        // Check day_seq to ensure we don't get stuck if barrier flips 0->1 immediately for next day
        struct timespec ts;
        while (g_running && atomic_load(&shm->sync.barrier_active) && 
                atomic_load(&shm->sync.day_seq) == barrier_day) {
            clock_gettime(CLOCK_MONOTONIC, &ts);
            ts.tv_sec += 1; // 1s timeout
            pthread_cond_timedwait(&shm->sync.cond_day_start, &shm->sync.mutex, &ts);
        }
    }

    pthread_mutex_unlock(&shm->sync.mutex);
}

/**
 * @brief Main worker loop.
 * 
 * Serves tickets from the shared queue for the assigned service type.
 *
 * @param[in] worker_id Unique ID of this worker.
 * @param[in] service_type Service type to handle.
 * @return EXIT_SUCCESS or EXIT_FAILURE.
 */
int worker_run(int worker_id, int service_type) {
    if (!g_shm_worker || g_semid_worker == -1) {
        LOG_FATAL("Worker globals not initialized");
        return EXIT_FAILURE;
    }

    sim_shm_t *shm = g_shm_worker;

    // Validation
    if (worker_id < 0 || (uint32_t)worker_id >= shm->params.n_workers) {
        LOG_FATAL("Worker ID %d out of range (n_workers=%u)", worker_id, shm->params.n_workers);
        return EXIT_FAILURE;
    }

    if (service_type < 0 || service_type >= SIM_MAX_SERVICE_TYPES) {
        LOG_FATAL("Invalid service type %d", service_type);
        return EXIT_FAILURE;
    }

    int day, hour, min;
    get_sim_time(shm, &day, &hour, &min);

    LOG_INFO("[Day %d %02d:%02d] Worker %d started (TID: %ld) type %d", day, hour, min, worker_id,
             (long)gettid(), service_type); // Using gettid if available or just logging

    // Register myself in SHM (Use PID as thread ID roughly or just placeholder)
    atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_FREE);
    atomic_store(&shm->workers[worker_id].pid, getpid());
    atomic_store(&shm->workers[worker_id].service_type, service_type);

    int last_synced_day = 0;

    // Main Loop
    while (g_running) {
        sync_check_day(shm, &last_synced_day);

        queue_status_t *q = &shm->queues[service_type];

        // Wait for a user using Condition Variable (Zero CPU Idle)
        pthread_mutex_lock(&q->mutex);
        while (atomic_load(&q->waiting_count) == 0 && g_running) {
             if (atomic_load(&shm->sync.barrier_active)) {
                 LOG_TRACE("Worker %d detected active barrier while waiting", worker_id);
                 break; // Sync barrier requested
             }
             LOG_TRACE("Worker %d waiting on cond_added for service %d", worker_id, service_type);
             
             struct timespec ts;
             clock_gettime(CLOCK_MONOTONIC, &ts);
             ts.tv_sec += 1;
             pthread_cond_timedwait(&q->cond_added, &q->mutex, &ts);
             
             LOG_TRACE("Worker %d woke up from cond_added", worker_id);
        }

        if (atomic_load(&shm->sync.barrier_active)) {
            pthread_mutex_unlock(&q->mutex);
            continue; // Handle sync
        }

        if (!g_running) {
            pthread_mutex_unlock(&q->mutex);
            break;
        }

        // We have acquired a user. Decrement the waiting count.
        atomic_fetch_sub(&q->waiting_count, 1);
        pthread_mutex_unlock(&q->mutex);

        LOG_DEBUG("Worker %d acquired a user task", worker_id);

        // Servicing
        atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_BUSY);

        // Pop Ticket
        unsigned int head = atomic_fetch_add(&q->head, 1);
        unsigned int idx = head % 128;

        // Wait for data (Should be immediate if logic is correct)
        uint32_t val = 0;
        int spin_count = 0;
        while ((val = atomic_load(&q->tickets[idx])) == 0) {
            if (!g_running)
                break;
            // Busy wait to avoid context switch latency
             if (++spin_count > 100000) {
                 usleep(1); // Emergency yield
                 spin_count = 0;
             }
        }
        if (val == 0)
            break; // shutting down

        // Clear slot
        atomic_store(&q->tickets[idx], 0);

        uint32_t ticket = val - 1;
        atomic_store(&shm->workers[worker_id].current_ticket, ticket);
        LOG_DEBUG("Worker %d retrieved ticket %u from shared queue", worker_id, ticket);

        get_sim_time(shm, &day, &hour, &min);
        // Working Hours Check: 08:00 to 17:00
        if (hour < 8 || hour >= 17) {
            if (atomic_load(&shm->workers[worker_id].state) != WORKER_STATUS_OFFLINE) {
                atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_OFFLINE);
            }
            // Just wait a bit and retry (or better, wait on cond variable with timeout?)
            usleep(10000);
            continue;
        }

        if (atomic_load(&shm->workers[worker_id].state) == WORKER_STATUS_OFFLINE) {
            atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_FREE);
        }

        LOG_DEBUG("[Day %d %02d:%02d] Worker %d servicing ticket %u...", day, hour, min, worker_id,
                  ticket);

        // Notify Users that I picked up the ticket (Use broadcast on served/change?)
        // The user waits for completion, but also waits to see who picked it up.
        // We can broadcast `cond_served` here too so users can check if their ticket is picked up.
        pthread_mutex_lock(&q->mutex);
        LOG_TRACE("Worker %d signaling cond_served (start of service)", worker_id);
        pthread_cond_broadcast(&q->cond_served);
        pthread_mutex_unlock(&q->mutex);

        // Simulate Service Time (e.g., 50ms - 500ms)
        // BUSY WAIT implementation for High CPU Load
        int service_time_ms = (int)po_rand_range_i64(50, 500);

        // Runtime Adaptation: Throttle if CPU is overloaded
        double cpu_util = -1.0;
        if (po_sysinfo_sampler_get(&cpu_util, NULL) == 0) {
            if (cpu_util > 95.0) { // Higher threshold
                 // service_time_ms = (int)(service_time_ms * 1.5);
            }
        }
        LOG_DEBUG("Worker %d simulating service for %d ms", worker_id, service_time_ms);

        // Busy wait loop
        // Burn CPU replaced by sleep to avoid starving User processes in simulation
        usleep((useconds_t)service_time_ms * 1000);

        // Done
        atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_FREE);
        atomic_store(&shm->workers[worker_id].current_ticket, 0xFFFFFFFF); // Invalid

        atomic_fetch_add(&shm->stats.total_services_completed, 1);
        atomic_fetch_add(&shm->queues[service_type].total_served, 1);

        // Notify Completion
        pthread_mutex_lock(&q->mutex);
        LOG_TRACE("Worker %d signaling cond_served (completion)", worker_id);
        pthread_cond_broadcast(&q->cond_served);
        pthread_mutex_unlock(&q->mutex);

        get_sim_time(shm, &day, &hour, &min);
        LOG_DEBUG("[Day %d %02d:%02d] Worker %d finished service ticket %u", day, hour, min,
                  worker_id, ticket);
    }

    LOG_INFO("Worker %d shutting down...", worker_id);
    atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_OFFLINE);

    return EXIT_SUCCESS;
}
