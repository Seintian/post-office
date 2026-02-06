#include <stdatomic.h>
#include <stdlib.h>

#include "../src/core/simulation/director/load_balance.h"
#include "../src/core/simulation/ipc/simulation_protocol.h"
#include "unity/unity_fixture.h"

static sim_shm_t *mock_shm = NULL;

TEST_GROUP(LOAD_BALANCE);

TEST_SETUP(LOAD_BALANCE) {
    mock_shm = calloc(1, sizeof(sim_shm_t) + (sizeof(worker_status_t) * 4));
    mock_shm->params.n_workers = 4;
}

TEST_TEAR_DOWN(LOAD_BALANCE) {
    if (mock_shm) {
        free(mock_shm);
        mock_shm = NULL;
    }
}

TEST(LOAD_BALANCE, NO_REBALANCE_WHEN_BALANCED) {
    load_balance_config_t cfg = {
        .enabled = true, .check_interval = 5, .imbalance_threshold = 200, .min_queue_depth = 5};
    load_balance_init(&cfg);

    // Both queues have 6 users (balanced)
    atomic_store(&mock_shm->queues[0].waiting_count, 6);
    atomic_store(&mock_shm->queues[1].waiting_count, 6);

    load_balance_stats_t stats = {0};
    int reassigned = load_balance_check(mock_shm, &stats);

    TEST_ASSERT_EQUAL_INT(0, reassigned);
    TEST_ASSERT_EQUAL_UINT32(1, stats.checks_performed);
}

TEST(LOAD_BALANCE, REBALANCE_TRIGGERED_ON_IMBALANCE) {
    load_balance_config_t cfg = {.enabled = true,
                                 .check_interval = 5,
                                 .imbalance_threshold = 200, // 2x
                                 .min_queue_depth = 3};
    load_balance_init(&cfg);

    // Queue 0: 10 users, Queue 1: 0 users (Ratio = max)
    atomic_store(&mock_shm->queues[0].waiting_count, 10);
    atomic_store(&mock_shm->queues[1].waiting_count, 0);

    // Worker 2 is idle and on Queue 1
    atomic_store(&mock_shm->workers[2].state, WORKER_STATUS_FREE);
    atomic_store(&mock_shm->workers[2].service_type, 1);

    load_balance_stats_t stats = {0};
    int reassigned = load_balance_check(mock_shm, &stats);

    TEST_ASSERT_EQUAL_INT(1, reassigned);
    TEST_ASSERT_EQUAL_UINT32(1, stats.rebalances_triggered);
    TEST_ASSERT_EQUAL_INT(0, atomic_load(&mock_shm->workers[2].service_type));
    TEST_ASSERT_EQUAL_INT(1, atomic_load(&mock_shm->workers[2].reassignment_pending));
}

TEST(LOAD_BALANCE, MIN_QUEUE_DEPTH_IGNORED) {
    load_balance_config_t cfg = {
        .enabled = true, .check_interval = 5, .imbalance_threshold = 200, .min_queue_depth = 10};
    load_balance_init(&cfg);

    // Queue 0: 8 users, Queue 1: 0 users
    // Ratio is high, but depth (8) < min_queue_depth (10)
    atomic_store(&mock_shm->queues[0].waiting_count, 8);
    atomic_store(&mock_shm->queues[1].waiting_count, 0);

    load_balance_stats_t stats = {0};
    int reassigned = load_balance_check(mock_shm, &stats);

    TEST_ASSERT_EQUAL_INT(0, reassigned);
}

TEST_GROUP_RUNNER(LOAD_BALANCE) {
    RUN_TEST_CASE(LOAD_BALANCE, NO_REBALANCE_WHEN_BALANCED);
    RUN_TEST_CASE(LOAD_BALANCE, REBALANCE_TRIGGERED_ON_IMBALANCE);
    RUN_TEST_CASE(LOAD_BALANCE, MIN_QUEUE_DEPTH_IGNORED);
}
