#define _POSIX_C_SOURCE 200809L

#include "worker_loop.h"
#include <postoffice/log/logger.h>
#include <utils/errors.h>
#include <postoffice/perf/cache.h>
#include <postoffice/random/random.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>
#include <postoffice/sysinfo/sysinfo.h>
#include "../../ipc/simulation_ipc.h"
#include "../../ipc/simulation_protocol.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>
#include <time.h>
#include <utils/signals.h>
#include <stdatomic.h>

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

static void get_sim_time(sim_shm_t* shm, int *d, int *h, int *m) {
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *d = (packed >> 16) & 0xFFFF;
    *h = (packed >> 8) & 0xFF;
    *m = packed & 0xFF;
}

// Internal init helper
static int worker_init(int worker_id, int service_type, sim_shm_t** out_shm, int* out_semid) {
    // Collect system information for optimizations
    po_sysinfo_t sysinfo;
    size_t cacheline_size = 64; // default
    if (po_sysinfo_collect(&sysinfo) == 0 && sysinfo.dcache_lnsize > 0) {
        cacheline_size = (size_t)sysinfo.dcache_lnsize;
    }

    if (po_metrics_init() != 0) {
        fprintf(stderr, "worker: metrics init failed\n");
    }

    int level = LOG_INFO;
    char *env = getenv("PO_LOG_LEVEL");
    if (env) {
        int l = po_logger_level_from_str(env);
        if (l != -1) level = l;
    }

    po_logger_config_t log_cfg = {
        .level = level,
        .ring_capacity = 4096,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = cacheline_size
    };
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
    sim_shm_t* shm = sim_ipc_shm_attach();
    if (!shm) {
        LOG_FATAL("worker: failed to attach SHM");
        return -1;
    }
    *out_shm = shm;

    // Validation
    if (worker_id < 0 || (uint32_t)worker_id >= shm->params.n_workers) {
        LOG_FATAL("Worker ID %d out of range (n_workers=%u)", worker_id, shm->params.n_workers);
        return -1;
    }

    if (service_type < 0 || service_type >= SIM_MAX_SERVICE_TYPES) {
        LOG_FATAL("Invalid service type %d", service_type);
        return -1;
    }

    // Get Semaphores
    int semid = sim_ipc_sem_get();
    if (semid == -1) {
        LOG_ERROR("worker %d: failed to get semaphores", worker_id);
        return -1;
    }
    *out_semid = semid;

    setup_signals();
    po_rand_seed_auto();

    return 0;
}

int worker_run(int worker_id, int service_type) {
    sim_shm_t* shm = NULL;
    int semid = -1;

    if (worker_init(worker_id, service_type, &shm, &semid) != 0) {
        if (shm) sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return EXIT_FAILURE;
    }

    int day, hour, min;
    get_sim_time(shm, &day, &hour, &min);

    LOG_INFO("[Day %d %02d:%02d] Worker %d started (PID: %d) type %d", 
             day, hour, min, worker_id, getpid(), service_type);

    // Register myself in SHM
    atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_FREE);
    atomic_store(&shm->workers[worker_id].pid, getpid());
    atomic_store(&shm->workers[worker_id].service_type, service_type);

    // Main Loop
    while (g_running) {
        // Wait for a user in my service queue
        struct sembuf sb = {
            .sem_num = (unsigned short)service_type,
            .sem_op = -1, // Decrement (Wait for user)
            .sem_flg = 0
        };

        // Blocking wait
        if (semop(semid, &sb, 1) == -1) {
            if (errno == EINTR) {
                if (!g_running) break;
                continue;
            }
            LOG_ERROR("worker %d: semop failed (errno=%d)", worker_id, errno);
            break;
        }

        if (!g_running) break;

        // Servicing
        atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_BUSY);

        // Pop Ticket
        queue_status_t *q = &shm->queues[service_type];
        unsigned int head = atomic_fetch_add(&q->head, 1);
        unsigned int idx = head % 128;

        // Wait for data
        uint32_t val = 0;
        int spin_count = 0;
        while ((val = atomic_load(&q->tickets[idx])) == 0) {
            if (!g_running) break;
            if (++spin_count > 1000) usleep(100);
        }
        if (val == 0) break; // shutting down

        // Clear slot
        atomic_store(&q->tickets[idx], 0);

        uint32_t ticket = val - 1;
        atomic_store(&shm->workers[worker_id].current_ticket, ticket);

        get_sim_time(shm, &day, &hour, &min);
        LOG_DEBUG("[Day %d %02d:%02d] Worker %d servicing ticket %u...", 
                 day, hour, min, worker_id, ticket);

        // Simulate Service Time (e.g., 50ms - 500ms)
        int service_time_ms = (int)po_rand_range_i64(50, 500);
        usleep((unsigned int)service_time_ms * 1000);

        // Done
        atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_FREE);
        // Reset current ticket? Not strictly needed but good for cleanliness
        // atomic_store(&shm->workers[worker_id].current_ticket, 0); 
        // Actually keep it until next valid? Or 0 to signal 'not serving'.
        // User checks 'current_ticket == ticket'. If we leave it, user 'served' logic might be confused?
        // User loop:
        // if (current_t == ticket) served=true;
        // then while(current_t == ticket) { still_serving=true }
        // So when we change it, user exits. 
        // If we change it to 0, user exits. Correct.
        atomic_store(&shm->workers[worker_id].current_ticket, 0xFFFFFFFF); // Invalid

        atomic_fetch_add(&shm->stats.total_services_completed, 1);
        atomic_fetch_add(&shm->queues[service_type].total_served, 1);

        get_sim_time(shm, &day, &hour, &min);
        LOG_DEBUG("[Day %d %02d:%02d] Worker %d finished service ticket %u", 
                 day, hour, min, worker_id, ticket);
    }

    LOG_INFO("Worker %d shutting down...", worker_id);
    atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_OFFLINE);

    sim_ipc_shm_detach(shm);
    po_logger_shutdown();
    po_metrics_shutdown();
    po_perf_shutdown(stdout);
    return EXIT_SUCCESS;
}
