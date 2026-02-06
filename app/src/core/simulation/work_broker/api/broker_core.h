#ifndef BROKER_CORE_H
#define BROKER_CORE_H

#include <postoffice/concurrency/threadpool.h>
#include <postoffice/log/logger.h>
#include <postoffice/net/poller.h>
#include <postoffice/priority_queue/priority_queue.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "ipc/simulation_ipc.h"

// --- Structures ---

typedef struct {
    uint32_t ticket_number;
    int is_vip;
    pid_t requester_pid;
    struct timespec arrival_time;
} broker_item_t;

typedef struct {
    // Queues
    po_priority_queue_t *queues[SIM_MAX_SERVICE_TYPES];
    pthread_mutex_t queue_mutexes[SIM_MAX_SERVICE_TYPES];

    // Runtime
    sim_shm_t *shm;
    int socket_fd;
    volatile sig_atomic_t shutdown_requested;
    threadpool_t *tp;
    poller_t *poller;

    // Config
    size_t pool_size;
} broker_ctx_t;

// --- Core Functions ---

/**
 * @brief Initialize the broker context (queues, shm, socket).
 * @return 0 on success, <0 on failure.
 */
int broker_init(broker_ctx_t *ctx, const char *loglevel, size_t pool_size);

/**
 * @brief Run the broker main loop.
 */
void broker_run(broker_ctx_t *ctx);

/**
 * @brief Cleanup broker resources.
 */
void broker_cleanup(broker_ctx_t *ctx);

#endif
