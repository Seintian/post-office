#define _POSIX_C_SOURCE 200809L

#include "worker_core.h"

#include <getopt.h>
#include <postoffice/concurrency/threadpool.h>
#include <postoffice/concurrency/waitgroup.h>
#include <postoffice/log/logger.h>
#include <postoffice/sort/sort.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strdup/sys headers might provide it, explicit include good
#include <unistd.h>

#include "../ipc/simulation_protocol.h"
#include "runtime/worker_loop.h"

// --- Internal Structs ---
typedef struct {
    int worker_id;
    int service_type;
    sim_shm_t *shm;
    worker_sync_t *sync_ctx;
    waitgroup_t *wg;
} worker_thread_context_t;

static void execution_task_wrapper(void *arg) {
    worker_thread_context_t *ctx = (worker_thread_context_t *)arg;
    LOG_DEBUG("Thread started for Worker %d", ctx->worker_id);
    run_worker_service_loop(ctx->worker_id, ctx->service_type, ctx->shm, ctx->sync_ctx);
    waitgroup_t *wg = ctx->wg;
    free(ctx);
    wg_done(wg);
}

void worker_parse_args(int argc, char **argv, worker_config_t *cfg) {
    if (!cfg)
        return;
    // Defaults
    cfg->worker_id = -1;
    cfg->service_type = -1;
    cfg->n_workers = 0;
    cfg->loglevel = NULL; // Will default to INFO in run logic if NULL, or handle here.
                          // worker.c original: "if (opt == 'l') setenv..."
                          // We should handle setenv here or passed via config.
                          // Original main setenv immediately. We'll do same.

    int opt;
    // Reset getopt
    optind = 1;

    while ((opt = getopt(argc, argv, "l:i:s:w:")) != -1) {
        char *endptr;
        long val;
        switch (opt) {
        case 'l':
            setenv("PO_LOG_LEVEL", optarg, 1);
            cfg->loglevel = optarg; // Just store pointer
            break;
        case 'i':
            val = strtol(optarg, &endptr, 10);
            if (*endptr == '\0' && val >= 0)
                cfg->worker_id = (int)val;
            break;
        case 's':
            val = strtol(optarg, &endptr, 10);
            if (*endptr == '\0' && val >= 0)
                cfg->service_type = (int)val;
            break;
        case 'w':
            val = strtol(optarg, &endptr, 10);
            if (*endptr == '\0' && val > 0)
                cfg->n_workers = (int)val;
            break;
        default:
            break;
        }
    }
}

int worker_run_simulation(const worker_config_t *cfg) {
    po_sort_init();

    // 1. Initialize Runtime Environment
    sim_shm_t *shm = initialize_worker_runtime();
    if (!shm) {
        LOG_ERROR("Failed to initialize worker runtime environment");
        return 1;
    }

    // 2. Spawn Workers
    if (cfg->n_workers > 0) {
        // Multi-threaded Mode
        threadpool_t *tp = tp_create((size_t)cfg->n_workers + 4, 0); // Unbounded queue
        if (!tp) {
            LOG_ERROR("Failed to create thread pool");
            teardown_worker_runtime(shm);
            return 1;
        }
        atomic_fetch_add(&shm->stats.connected_threads, (uint32_t)cfg->n_workers + 1);
        atomic_fetch_add(&shm->stats.active_threads, 1); // Main thread is active
        tp_set_active_counter(tp, &shm->stats.active_threads);

        waitgroup_t *wg = wg_create();

        // Initialize Synchronization Context
        worker_sync_t sync_ctx;
        pthread_barrier_init(&sync_ctx.barrier, NULL, (unsigned)cfg->n_workers);
        sync_ctx.current_day = 0;
        sync_ctx.shutdown_signal = 0;

        LOG_INFO("Launching %d worker threads...", cfg->n_workers);

        for (int i = 0; i < cfg->n_workers; i++) {
            worker_thread_context_t *ctx = malloc(sizeof(worker_thread_context_t));
            ctx->worker_id = i;
            // Round-robin assignment for simulation
            ctx->service_type = i % SIM_MAX_SERVICE_TYPES;
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
        atomic_fetch_sub(&shm->stats.connected_threads, (uint32_t)cfg->n_workers + 1);
        tp_destroy(tp, true);
        pthread_barrier_destroy(&sync_ctx.barrier);

    } else if (cfg->worker_id != -1 && cfg->service_type != -1) {
        // Single-process Mode (Legacy/Debug)
        worker_sync_t sync_ctx;
        pthread_barrier_init(&sync_ctx.barrier, NULL, 1);
        sync_ctx.current_day = 0;
        sync_ctx.shutdown_signal = 0;

        run_worker_service_loop(cfg->worker_id, cfg->service_type, shm, &sync_ctx);
        pthread_barrier_destroy(&sync_ctx.barrier);
    } else {
        LOG_ERROR("Invalid configuration. Need -w <n> OR -i <id> -s <type>");
        teardown_worker_runtime(shm);
        return 1;
    }

    // 3. Cleanup
    teardown_worker_runtime(shm);
    po_sort_finish();
    return 0;
}
