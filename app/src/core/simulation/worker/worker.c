/**
 * @file worker.c
 * @brief Worker Process Implementation.
 */
#define _POSIX_C_SOURCE 200809L

#include <postoffice/log/logger.h>
#include <utils/errors.h>
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

int main(int argc, char** argv) {
    // Expected args: ./worker <worker_id> <service_type_A> [<service_type_B>...]
    // For simplicity, assume 1 arg: worker_id, and it serves ALL or fixed type.
    // Let's assume Director passes: --id <id> --service <type>
    
    int worker_id = 0;
    int service_type = 0; // Default service

    int opt;
    while ((opt = getopt(argc, argv, "i:s:")) != -1) {
        switch (opt) {
            case 'i': worker_id = atoi(optarg); break;
            case 's': service_type = atoi(optarg); break;
            default: break;
        }
    }

    // 1. Init Logger
    po_logger_config_t log_cfg = {
        .level = LOG_INFO,
        .ring_capacity = 1024,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = 64
    };
    if (po_logger_init(&log_cfg) != 0) return 1;
    
    // Use shared logfile in append mode
    po_logger_add_sink_file("logs/workers.log", true);

    // 2. Attach SHM
    sim_shm_t* shm = sim_ipc_shm_attach();
    if (!shm) {
        LOG_ERROR("worker %d: failed to attach SHM", worker_id);
        po_logger_shutdown();
        return 1;
    }

    // 3. Get Semaphores
    int semid = sim_ipc_sem_get();
    if (semid == -1) {
        LOG_ERROR("worker %d: failed to get semaphores", worker_id);
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return 1;
    }

    setup_signals();
    
    sim_time_t *t = &shm->time_control;
    LOG_INFO("[Day %d %02d:%02d] Worker %d started (PID: %d) type %d", 
             atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute),
             worker_id, getpid(), service_type);

    // Register myself in SHM
    if (worker_id < SIM_MAX_WORKERS) {
        atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_FREE);
        shm->workers[worker_id].pid = getpid();
        atomic_store(&shm->workers[worker_id].service_type, service_type);
    }

    // 4. Main Loop
    while (g_running) {
        // Wait for a user in my service queue
        struct sembuf sb = {
            .sem_num = (unsigned short)service_type,
            .sem_op = -1, // Decrement (Wait for user)
            .sem_flg = 0
        };

        // Blocking wait (or use IPC_NOWAIT and sleep to be responsive to signals)
        if (semop(semid, &sb, 1) == -1) {
            if (errno == EINTR) continue;
            LOG_ERROR("worker %d: semop failed (errno=%d)", worker_id, errno);
            break;
        }

        // We grabbed a user!
        atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_BUSY);
        
        // Log Start
        LOG_INFO("[Day %d %02d:%02d] Worker %d servicing ticket...", 
                 atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute), worker_id);
        
        // Simulate Service Time (e.g., 50ms - 500ms)
        // In simulation time, we should probably check sim_shm->time_control.
        // For now, allow simple real-time sleep.
        int service_time_ms = 100 + (rand() % 400); 
        usleep((unsigned int)service_time_ms * 1000);

        // Done
        atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_FREE);
        atomic_fetch_add(&shm->stats.total_services_completed, 1);
        atomic_fetch_add(&shm->queues[service_type].total_served, 1);
        
        // Decrement waiting count in SHM stats
        atomic_fetch_sub(&shm->queues[service_type].waiting_count, 1);

        LOG_INFO("[Day %d %02d:%02d] Worker %d finished service", 
                 atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute), worker_id);
    }

    LOG_INFO("Worker %d shutting down...", worker_id);
    atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_OFFLINE);

    sim_ipc_shm_detach(shm);
    po_logger_shutdown();
    return 0;
}
