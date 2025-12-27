/**
 * @file worker.c
 * @brief Worker Process Entry Point.
 * 
 * This program acts as a launcher for worker threads. It parses configuration
 * and spawns the required number of worker threads to service user requests.
 */

#define _POSIX_C_SOURCE 200809L

#include "runtime/worker_loop.h"
#include "../ipc/simulation_protocol.h"

#include <postoffice/log/logger.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <postoffice/concurrency/threadpool.h>
#include <postoffice/concurrency/waitgroup.h>
#include <postoffice/sort/sort.h>

typedef struct {
    int worker_id;
    int service_type;
    sim_shm_t *shm;
    worker_sync_t *sync_ctx;
    waitgroup_t *wg;
} worker_thread_context_t;

/**
 * @brief Thread entry point for an individual worker.
 */
static void execution_task_wrapper(void* arg) {
    worker_thread_context_t* ctx = (worker_thread_context_t*)arg;
    run_worker_service_loop(ctx->worker_id, ctx->service_type, ctx->shm, ctx->sync_ctx);
    waitgroup_t *wg = ctx->wg;
    free(ctx);
    wg_done(wg);
}

/**
 * @brief Parses command line arguments for the worker process.
 */
static void parse_cli_args(int argc, char** argv, int* worker_id, int* service_type, int* n_workers) {
    int opt;
    while ((opt = getopt(argc, argv, "l:i:s:w:")) != -1) {
        char *endptr;
        long val;
        switch (opt) {
            case 'l':
                setenv("PO_LOG_LEVEL", optarg, 1);
                break;
            case 'i': 
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val >= 0) *worker_id = (int)val;
                break;
            case 's': 
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val >= 0) *service_type = (int)val;
                break;
            case 'w':
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val > 0) *n_workers = (int)val;
                break;
            default: break;
        }
    }
}

int main(int argc, char** argv) {
    int worker_id = -1;
    int service_type = -1;
    int n_workers = 0;

    parse_cli_args(argc, argv, &worker_id, &service_type, &n_workers);
    po_sort_init();

    // 1. Initialize Runtime Environment
    sim_shm_t *shm = initialize_worker_runtime();
    if (!shm) {
        LOG_ERROR("Failed to initialize worker runtime environment");
        return 1;
    }

    // 2. Spawn Workers
    if (n_workers > 0) {
        // Multi-threaded Mode
        threadpool_t *tp = tp_create((size_t)n_workers + 4, 0); // Unbounded queue
        if (!tp) {
            LOG_ERROR("Failed to create thread pool");
            teardown_worker_runtime(shm);
            return 1;
        }
        atomic_fetch_add(&shm->stats.connected_threads, (uint32_t)n_workers + 1);
        atomic_fetch_add(&shm->stats.active_threads, 1); // Main thread is active
        tp_set_active_counter(tp, &shm->stats.active_threads);

        waitgroup_t *wg = wg_create();

        // Initialize Synchronization Context
        worker_sync_t sync_ctx;
        pthread_barrier_init(&sync_ctx.barrier, NULL, (unsigned)n_workers);
        sync_ctx.current_day = 0;
        sync_ctx.shutdown_signal = 0;

        LOG_INFO("Launching %d worker threads...", n_workers);

        for (int i = 0; i < n_workers; i++) {
            worker_thread_context_t* ctx = malloc(sizeof(worker_thread_context_t));
            ctx->worker_id = i;
            ctx->service_type = i % SIM_MAX_SERVICE_TYPES; // Round-robin assignment
            ctx->shm = shm;
            ctx->sync_ctx = &sync_ctx; // Pass reference
            ctx->wg = wg;
            
            wg_add(wg, 1);
            tp_submit(tp, execution_task_wrapper, ctx);
        }

        // Wait for completion
        wg_wait(wg);
        
        wg_destroy(wg);
        atomic_fetch_sub(&shm->stats.active_threads, 1);
        atomic_fetch_sub(&shm->stats.connected_threads, (uint32_t)n_workers + 1);
        tp_destroy(tp, true);
        pthread_barrier_destroy(&sync_ctx.barrier);

    } else if (worker_id != -1 && service_type != -1) {
        // Single-process Mode (Legacy/Debug)
        // Create dummy sync context for single-process fallback
        worker_sync_t sync_ctx;
        pthread_barrier_init(&sync_ctx.barrier, NULL, 1); 
        sync_ctx.current_day = 0;
        sync_ctx.shutdown_signal = 0;

        run_worker_service_loop(worker_id, service_type, shm, &sync_ctx);
        pthread_barrier_destroy(&sync_ctx.barrier);
    } else {
        LOG_ERROR("Usage: %s -w <n_workers> OR -i <id> -s <type>", argv[0]);
        teardown_worker_runtime(shm);
        return 1;
    }

    // 3. Cleanup
    teardown_worker_runtime(shm);
    po_sort_finish();
    return 0;
}
