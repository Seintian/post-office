/**
 * @file load_balance.h
 * @brief Dynamic load balancing via worker reassignment.
 * @ingroup director
 *
 * Monitors queue depths across service types and triggers worker
 * reassignment when imbalance exceeds configured thresholds.
 */
#ifndef DIRECTOR_LOAD_BALANCE_H
#define DIRECTOR_LOAD_BALANCE_H

#include <stdbool.h>
#include <stdint.h>

#include "../ipc/simulation_protocol.h"

/**
 * @brief Load balancing configuration.
 */
typedef struct {
    bool enabled;
    uint32_t check_interval;      /**< Check frequency in sim minutes */
    uint32_t imbalance_threshold; /**< Ratio to trigger (e.g., 200 = 2x) */
    uint32_t min_queue_depth;     /**< Ignore queues below this depth */
} load_balance_config_t;

/**
 * @brief Statistics for load balancing operations.
 */
typedef struct {
    uint32_t checks_performed;
    uint32_t rebalances_triggered;
    uint32_t workers_reassigned;
} load_balance_stats_t;

/**
 * @brief Initialize load balancing state.
 * @param cfg Configuration to apply.
 */
void load_balance_init(load_balance_config_t *cfg);

/**
 * @brief Check for queue imbalance and reassign workers if needed.
 * @param shm Pointer to shared memory.
 * @param stats Statistics output (can be NULL).
 * @return Number of workers reassigned (0 if none).
 */
int load_balance_check(sim_shm_t *shm, load_balance_stats_t *stats);

/**
 * @brief Log current load balancing statistics.
 * @param stats Statistics to log.
 */
void load_balance_log_stats(const load_balance_stats_t *stats);

#endif /* DIRECTOR_LOAD_BALANCE_H */
