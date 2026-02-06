/**
 * @file load_balance.c
 * @brief Dynamic load balancing implementation.
 */
#define _POSIX_C_SOURCE 200809L

#include "load_balance.h"

#include <postoffice/log/logger.h>
#include <stdatomic.h>
#include <string.h>

/* Module-level configuration */
static load_balance_config_t g_config = {0};

void load_balance_init(load_balance_config_t *cfg) {
    if (cfg) {
        memcpy(&g_config, cfg, sizeof(g_config));
    }
    LOG_DEBUG("Load balancing %s (interval=%u, threshold=%u%%, min_depth=%u)",
              g_config.enabled ? "enabled" : "disabled", g_config.check_interval,
              g_config.imbalance_threshold, g_config.min_queue_depth);
}

/**
 * @brief Find the queue with the highest waiting count.
 */
static int find_overloaded_queue(sim_shm_t *shm, uint32_t *max_count) {
    int max_idx = -1;
    *max_count = 0;

    for (int i = 0; i < SIM_MAX_SERVICE_TYPES; i++) {
        uint32_t count = atomic_load(&shm->queues[i].waiting_count);
        if (count > *max_count) {
            *max_count = count;
            max_idx = i;
        }
    }
    return max_idx;
}

/**
 * @brief Find the queue with the lowest waiting count.
 */
static int find_underloaded_queue(sim_shm_t *shm, uint32_t *min_count) {
    int min_idx = -1;
    *min_count = UINT32_MAX;

    for (int i = 0; i < SIM_MAX_SERVICE_TYPES; i++) {
        uint32_t count = atomic_load(&shm->queues[i].waiting_count);
        if (count < *min_count) {
            *min_count = count;
            min_idx = i;
        }
    }
    return min_idx;
}

/**
 * @brief Find an idle worker assigned to a given service type.
 * @return Worker index or -1 if none found.
 */
static int find_idle_worker_for_service(sim_shm_t *shm, int service_type, uint32_t n_workers) {
    for (uint32_t i = 0; i < n_workers; i++) {
        int state = atomic_load(&shm->workers[i].state);
        int svc = atomic_load(&shm->workers[i].service_type);

        if (state == WORKER_STATUS_FREE && svc == service_type) {
            return (int)i;
        }
    }
    return -1;
}

int load_balance_check(sim_shm_t *shm, load_balance_stats_t *stats) {
    if (!g_config.enabled || !shm) {
        return 0;
    }

    if (stats) {
        stats->checks_performed++;
    }

    uint32_t max_count = 0;
    uint32_t min_count = 0;
    int overloaded = find_overloaded_queue(shm, &max_count);
    int underloaded = find_underloaded_queue(shm, &min_count);

    /* Sanity checks */
    if (overloaded < 0 || underloaded < 0 || overloaded == underloaded) {
        return 0;
    }

    /* Ignore if max queue is below minimum depth threshold */
    if (max_count < g_config.min_queue_depth) {
        return 0;
    }

    /* Check imbalance ratio: threshold of 200 means max >= 2x min */
    /* Avoid divide-by-zero: treat min=0 as significant imbalance if max >= min_depth */
    uint32_t ratio;
    if (min_count == 0) {
        ratio = (max_count >= g_config.min_queue_depth) ? 1000 : 0;
    } else {
        ratio = (max_count * 100) / min_count;
    }

    if (ratio < g_config.imbalance_threshold) {
        return 0; /* No significant imbalance */
    }

    LOG_DEBUG("Load imbalance detected: queue[%d]=%u vs queue[%d]=%u (ratio=%u%%)", overloaded,
              max_count, underloaded, min_count, ratio);

    /* Find an idle worker on the underloaded queue to reassign */
    uint32_t n_workers = shm->params.n_workers;
    int worker_idx = find_idle_worker_for_service(shm, underloaded, n_workers);

    if (worker_idx < 0) {
        LOG_TRACE("No idle workers on queue[%d] to reassign", underloaded);
        return 0;
    }

    /* Perform reassignment */
    LOG_INFO("Load balance: reassigning worker %d from queue %d to queue %d", worker_idx,
             underloaded, overloaded);

    atomic_store(&shm->workers[worker_idx].service_type, overloaded);
    atomic_store(&shm->workers[worker_idx].reassignment_pending, 1);

    /* Wake workers on the overloaded queue */
    pthread_mutex_lock(&shm->queues[overloaded].mutex);
    pthread_cond_broadcast(&shm->queues[overloaded].cond_added);
    pthread_mutex_unlock(&shm->queues[overloaded].mutex);

    if (stats) {
        stats->rebalances_triggered++;
        stats->workers_reassigned++;
    }

    return 1;
}

void load_balance_log_stats(const load_balance_stats_t *stats) {
    if (!stats) {
        return;
    }
    LOG_INFO("Load Balance Stats: checks=%u, rebalances=%u, workers_moved=%u",
             stats->checks_performed, stats->rebalances_triggered, stats->workers_reassigned);
}
