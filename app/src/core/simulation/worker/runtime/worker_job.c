#define _POSIX_C_SOURCE 200809L
#include "worker_job.h"
#include "ipc/sim_client.h"

#include <postoffice/log/logger.h>
#include <unistd.h>
#include <time.h>
#include <postoffice/random/random.h>

void worker_job_simulate(int worker_id, int service_type, uint32_t ticket, sim_shm_t *shm) {
    // 1. Update Status
    atomic_store(&shm->workers[worker_id].current_ticket, ticket);
    atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_BUSY);

    int d, h, m;
    sim_client_read_time(shm, &d, &h, &m);
    LOG_INFO("[Day %d %02d:%02d] Worker %d Started Serving Ticket #%u", d, h, m, worker_id, ticket);

    // 2. Simulate Work (Variable duration)
    // Calc duration based on service type?
    // Using random for now (100ms - 500ms)
    unsigned int duration_ms = (unsigned int)po_rand_range_i64(100, 500); 

    // We should ideally sleep in simulation time, but for real-time factor scaling:
    if (shm->params.tick_nanos > 0) {
        // Sleep in 10ms chunks to allow interruption
        unsigned int slept_ms = 0;
        while (slept_ms < duration_ms) {
            usleep(10000);
            slept_ms += 10;
            
            // Periodically check if office closed
            if (slept_ms % 50 == 0) {
                sim_client_read_time(shm, &d, &h, &m);
                if (h >= 17) {
                    LOG_WARN("Worker %d interrupted by Office Close (Serving Ticket #%u)", worker_id, ticket);
                    break;
                }
            }
        }
    }

    LOG_DEBUG("Worker %d performing service (%u ms)", worker_id, duration_ms);

    // 3. Complete
    sim_client_read_time(shm, &d, &h, &m);
    LOG_INFO("Worker %d Finished Ticket #%u (%u ms)", worker_id, ticket, duration_ms);

    atomic_store(&shm->workers[worker_id].current_ticket, 0);
    atomic_store(&shm->workers[worker_id].state, WORKER_STATUS_FREE);
    atomic_fetch_add(&shm->stats.total_services_completed, 1);

    // Notify queue (that we are free or ticket is done)
    queue_status_t *q = &shm->queues[service_type];
    pthread_mutex_lock(&q->mutex);
    atomic_store(&q->last_finished_ticket, ticket); // Persistent record of completion
    pthread_cond_broadcast(&q->cond_served);
    pthread_mutex_unlock(&q->mutex);
    // Also cond_added is used for workers to sleep, but here we wake users waiting on cond_served.
}
