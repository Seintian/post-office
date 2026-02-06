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

/**
 * @brief Analysis result structure.
 */
typedef struct {
    int overloaded_queue_idx;
    uint32_t overloaded_count;
    int underloaded_queue_idx;
    uint32_t underloaded_count;
    uint32_t ratio;
    bool should_rebalance;
} lb_analysis_t;

/**
 * @brief Analyze the current state of queues to determine if rebalancing is needed.
 * This function is pure (no side effects on the system state).
 */
static lb_analysis_t load_balance_analyze(sim_shm_t *shm) {
    lb_analysis_t result = {-1, 0, -1, 0, 0, false};

    if (!g_config.enabled || !shm) {
        return result;
    }

    result.overloaded_queue_idx = find_overloaded_queue(shm, &result.overloaded_count);
    result.underloaded_queue_idx = find_underloaded_queue(shm, &result.underloaded_count);

    /* Sanity checks */
    if (result.overloaded_queue_idx < 0 || result.underloaded_queue_idx < 0 ||
        result.overloaded_queue_idx == result.underloaded_queue_idx) {
        return result;
    }

    /* Ignore if max queue is below minimum depth threshold */
    if (result.overloaded_count < g_config.min_queue_depth) {
        return result;
    }

    /* Check imbalance ratio */
    if (result.underloaded_count == 0) {
        result.ratio = (result.overloaded_count >= g_config.min_queue_depth) ? 1000 : 0;
    } else {
        result.ratio = (result.overloaded_count * 100) / result.underloaded_count;
    }

    if (result.ratio >= g_config.imbalance_threshold) {
        result.should_rebalance = true;
    }

    return result;
}

/**
 * @brief Apply rebalancing based on analysis result.
 * This function performs the side effects (worker reassignment).
 */
static int load_balance_apply(sim_shm_t *shm, const lb_analysis_t *analysis,
                              load_balance_stats_t *stats) {
    LOG_DEBUG("Load imbalance detected: queue[%d]=%u vs queue[%d]=%u (ratio=%u%%)",
              analysis->overloaded_queue_idx, analysis->overloaded_count,
              analysis->underloaded_queue_idx, analysis->underloaded_count, analysis->ratio);

    /* Find an idle worker on the underloaded queue to reassign */
    uint32_t n_workers = shm->params.n_workers;
    int worker_idx = find_idle_worker_for_service(shm, analysis->underloaded_queue_idx, n_workers);

    if (worker_idx < 0) {
        LOG_TRACE("No idle workers on queue[%d] to reassign", analysis->underloaded_queue_idx);
        return 0;
    }

    /* Perform reassignment */
    LOG_INFO("Load balance: reassigning worker %d from queue %d to queue %d", worker_idx,
             analysis->underloaded_queue_idx, analysis->overloaded_queue_idx);

    atomic_store(&shm->workers[worker_idx].service_type, analysis->overloaded_queue_idx);
    atomic_store(&shm->workers[worker_idx].reassignment_pending, 1);

    /* Wake workers on the overloaded queue */
    int overloaded = analysis->overloaded_queue_idx;
    pthread_mutex_lock(&shm->queues[overloaded].mutex);
    pthread_cond_broadcast(&shm->queues[overloaded].cond_added);
    pthread_mutex_unlock(&shm->queues[overloaded].mutex);

    if (stats) {
        stats->rebalances_triggered++;
        stats->workers_reassigned++;
    }

    return 1;
}

int load_balance_check(sim_shm_t *shm, load_balance_stats_t *stats) {
    if (stats) {
        stats->checks_performed++;
    }

    lb_analysis_t analysis = load_balance_analyze(shm);

    if (analysis.should_rebalance) {
        return load_balance_apply(shm, &analysis, stats);
    }

    return 0;
}

void load_balance_log_stats(const load_balance_stats_t *stats) {
    if (!stats) {
        return;
    }
    LOG_INFO("Load Balance Stats: checks=%u, rebalances=%u, workers_moved=%u",
             stats->checks_performed, stats->rebalances_triggered, stats->workers_reassigned);
}
