#define _POSIX_C_SOURCE 200809L

#include <postoffice/log/logger.h>
#include <utils/errors.h>
#include <postoffice/perf/cache.h>
#include <postoffice/random/random.h>
#include "../ipc/simulation_ipc.h"
#include "../ipc/simulation_protocol.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>
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

static void get_sim_time(sim_shm_t* shm, int *d, int *h, int *m) {
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *d = (packed >> 16) & 0xFFFF;
    *h = (packed >> 8) & 0xFF;
    *m = packed & 0xFF;
}

int main(int argc, char** argv) {
    int worker_id = -1;
    int service_type = -1;

    int opt;
    while ((opt = getopt(argc, argv, "i:s:")) != -1) {
        char *endptr;
        long val;
        switch (opt) {
            case 'i': 
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val >= 0) worker_id = (int)val;
                break;
            case 's': 
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val >= 0) service_type = (int)val;
                break;
            default: break;
        }
    }

    // 1. Init Logger
    po_logger_config_t log_cfg = {
        .level = LOG_INFO,
        .ring_capacity = 1024,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = PO_CACHE_LINE_MAX
    };
    if (po_logger_init(&log_cfg) != 0) return 1;

    // Use shared logfile in append mode
    if (po_logger_add_sink_file("logs/workers.log", true) != 0) {
        LOG_WARN("Failed to add log file sink");
    }

    // 2. Attach SHM
    sim_shm_t* shm = sim_ipc_shm_attach();
    if (!shm) {
        LOG_FATAL("worker: failed to attach SHM");
        po_logger_shutdown();
        return 1;
    }

    // 3. Validation
    if (worker_id < 0 || (uint32_t)worker_id >= shm->params.n_workers) {
        LOG_FATAL("Worker ID %d out of range (n_workers=%u)", worker_id, shm->params.n_workers);
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        exit(EXIT_FAILURE);
    }

    if (service_type < 0 || service_type >= SIM_MAX_SERVICE_TYPES) {
        LOG_FATAL("Invalid service type %d", service_type);
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        exit(EXIT_FAILURE);
    }

    // 4. Get Semaphores
    int semid = sim_ipc_sem_get();
    if (semid == -1) {
        LOG_ERROR("worker %d: failed to get semaphores", worker_id);
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return 1;
    }

    setup_signals();
    po_rand_seed_auto();

    int day, hour, min;
    get_sim_time(shm, &day, &hour, &min);

    LOG_INFO("[Day %d %02d:%02d] Worker %d started (PID: %d) type %d", 
             day, hour, min, worker_id, getpid(), service_type);

    // Register myself in SHM
    atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_FREE);
    atomic_store(&shm->workers[worker_id].pid, getpid());
    atomic_store(&shm->workers[worker_id].service_type, service_type);

    // 5. Main Loop
    while (g_running) {
        // Wait for a user in my service queue
        struct sembuf sb = {
            .sem_num = (unsigned short)service_type,
            .sem_op = -1, // Decrement (Wait for user)
            .sem_flg = 0
        };

        // Blocking wait
        if (semop(semid, &sb, 1) == -1) {
            if (errno == EINTR) continue;
            LOG_ERROR("worker %d: semop failed (errno=%d)", worker_id, errno);
            break;
        }

        // We grabbed a user!
        atomic_fetch_sub(&shm->queues[service_type].waiting_count, 1);

        atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_BUSY);
        
        get_sim_time(shm, &day, &hour, &min);
        LOG_INFO("[Day %d %02d:%02d] Worker %d servicing ticket...", 
                 day, hour, min, worker_id);

        // Simulate Service Time (e.g., 50ms - 500ms)
        int service_time_ms = (int)po_rand_range_i64(50, 500);
        usleep((unsigned int)service_time_ms * 1000);

        // Done
        atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_FREE);
        atomic_fetch_add(&shm->stats.total_services_completed, 1);
        atomic_fetch_add(&shm->queues[service_type].total_served, 1);

        get_sim_time(shm, &day, &hour, &min);
        LOG_INFO("[Day %d %02d:%02d] Worker %d finished service", 
                 day, hour, min, worker_id);
    }

    LOG_INFO("Worker %d shutting down...", worker_id);
    atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_OFFLINE);

    sim_ipc_shm_detach(shm);
    po_logger_shutdown();
    return 0;
}
