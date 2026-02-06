/**
 * @file worker_loop.c
 * @brief Implementation of the Worker service loop.
 */

#define _POSIX_C_SOURCE 200809L

#include "worker_loop.h"

#include <postoffice/log/logger.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/net/net.h>
#include <postoffice/random/random.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utils/signals.h>

#include "ipc/sim_client.h"
#include "ipc/simulation_ipc.h"
#include "worker_job.h"

static volatile sig_atomic_t g_shutdown = 0;
static void on_sig(int s, siginfo_t *i, void *c) {
    (void)s;
    (void)i;
    (void)c;
    g_shutdown = 1;
}

sim_shm_t *initialize_worker_runtime(void) {
    po_metrics_init(0, 0, 0);

    po_sysinfo_t si;
    size_t cache =
        (po_sysinfo_collect(&si) == 0 && si.dcache_lnsize > 0) ? (size_t)si.dcache_lnsize : 64;

    char *lvl = getenv("PO_LOG_LEVEL");
    po_logger_init(&(po_logger_config_t){.level = (lvl && po_logger_level_from_str(lvl) != -1)
                                                      ? po_logger_level_from_str(lvl)
                                                      : LOG_INFO,
                                         .ring_capacity = 256,
                                         .consumers = 1,
                                         .cacheline_bytes = cache});
    po_logger_add_sink_file("logs/workers.log", true);
    // po_logger_add_sink_console(false);

    sim_shm_t *shm = sim_ipc_shm_attach();
    if (!shm) {
        LOG_FATAL("Worker failed to attach to Shared Memory! Cannot continue.");
        return NULL; // Should not reach here due to abort
    }

    sim_client_setup_signals(on_sig);
    po_rand_seed_auto();
    return shm;
}

void teardown_worker_runtime(sim_shm_t *shm) {
    if (shm)
        sim_ipc_shm_detach(shm);
    po_logger_shutdown();
}

static uint32_t retrieve_next_ticket(int service, sim_shm_t *shm) {
    queue_status_t *q = &shm->queues[service];

    pthread_mutex_lock(&q->mutex);
    while (!g_shutdown) {
        if (!atomic_load(&shm->time_control.sim_active)) {
            // If sim paused/ended?
            // Actually barrier usage handles daily pause.
            // But if day ended, we should break?
        }

        // Check head
        unsigned int head = atomic_load(&q->head);
        unsigned int tail = atomic_load(&q->tail);

        if (head != tail) {
            // Check hours first
            int d, h, m;
            sim_client_read_time(shm, &d, &h, &m);
            if (h >= 17) {
                // Office closed, do not pick up new tickets.
                pthread_mutex_unlock(&q->mutex);
                return 0;
            }

            // Something in queue
            unsigned int idx = head % 128;
            uint32_t ticket = atomic_load(&q->tickets[idx]);
            if (ticket != 0) {
                atomic_store(&q->tickets[idx], 0);
                atomic_fetch_add(&q->head, 1);
                atomic_fetch_sub(&q->waiting_count, 1);
                pthread_mutex_unlock(&q->mutex);
                return ticket - 1; // Decode 1-based
            }
        }

        // Wait
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait(&q->cond_added, &q->mutex, &ts);

        // Check barrier sync from client logic indirectly or loop break
        if (atomic_load(&shm->sync.barrier_active)) {
            pthread_mutex_unlock(&q->mutex);
            return 0; // Special case to yield to barrier
        }
    }
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

int run_worker_service_loop(int worker_id, int service_type, sim_shm_t *shm,
                            worker_sync_t *sync_ctx) {
    if (!shm || !sync_ctx)
        return 1;

    int last_day = 0;
    int d, h, m;
    while (!g_shutdown) {
        // 1. Enter Process-Local Barrier
        int rc = pthread_barrier_wait(&sync_ctx->barrier);

        // 2. Serial thread performs Global Sync
        if (rc == PTHREAD_BARRIER_SERIAL_THREAD) {
            int day_out = last_day;
            volatile int shut_out = g_shutdown;
            // Wait for Global Barrier (Process Count 1)
            sim_client_wait_barrier(shm, &day_out, &shut_out);

            // Update Shared Context
            sync_ctx->current_day = day_out;
            sync_ctx->shutdown_signal = shut_out;
        }

        // 3. Wait for Serial thread to finish Global Sync
        pthread_barrier_wait(&sync_ctx->barrier);

        // 4. Update local state from Shared Context
        last_day = sync_ctx->current_day;
        if (sync_ctx->shutdown_signal) {
            g_shutdown = 1;
        }

        if (g_shutdown)
            break;

        sim_client_read_time(shm, &d, &h, &m);
        LOG_INFO("[Day %d %02d:%02d] Worker %d Online (Type: %d)", d, h, m, worker_id,
                 service_type);

        while (!g_shutdown && !atomic_load(&shm->sync.barrier_active)) {
            uint32_t ticket = retrieve_next_ticket(service_type, shm);
            if (ticket > 0) {
                LOG_DEBUG("Worker %d acquiring ticket...", worker_id);
                worker_job_simulate(worker_id, service_type, ticket, shm);
            } else {
                if (atomic_load(&shm->sync.barrier_active))
                    break;
                // No tickets and no barrier - yield
                if (shm->params.tick_nanos > 0)
                    usleep(10000);
                else
                    sched_yield();
            }
        }

        LOG_INFO("Worker %d Day Ended (Reason: %s)", worker_id,
                 atomic_load(&shm->sync.barrier_active) ? "Barrier" : "Shutdown");
        // Yield to prevent spastic logging if Director cycles fast
        if (shm->params.tick_nanos > 0)
            usleep(10000);
        else
            sched_yield();
    }
    return 0;
}
