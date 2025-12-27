#define _POSIX_C_SOURCE 200809L
#include "users_spawn.h"
#include "../../user/runtime/user_loop.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <postoffice/log/logger.h>
#include <postoffice/concurrency/threadpool.h>
#include <postoffice/net/net.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <postoffice/concurrency/waitgroup.h>

static user_slot_t g_user_slots[MAX_USER_CAPACITY];
static sim_shm_t *g_shm = NULL; // Save for thread tracking
static uint32_t g_last_threads_count = 0;
static waitgroup_t *g_user_wg = NULL;
static threadpool_t *g_user_thread_pool = NULL;

typedef struct {
    int user_id;
    int service_type;
    int slot;  // Actual slot index, not recalculated
    sim_shm_t *shm;
} user_task_ctx_t;

void users_spawn_init(sim_shm_t *shm, size_t pool_size) {
    g_shm = shm;
    LOG_INFO("Initializing User Thread Pool (PoolSize: %zu, MaxCapacity: %d)", pool_size, MAX_USER_CAPACITY);
    for (int i = 0; i < MAX_USER_CAPACITY; i++) {
        atomic_init(&g_user_slots[i].is_occupied, false);
        atomic_init(&g_user_slots[i].should_continue_running, false);
    }

    g_user_wg = wg_create();

    po_sysinfo_t info;
    size_t cores;
    if (po_sysinfo_collect(&info) == 0 && info.physical_cores > 0) {
        cores = (size_t)info.physical_cores;
    } else {
        cores = (size_t)sysconf(_SC_NPROCESSORS_ONLN);
    }

    // Default to pool_size + cores if pool_size is small, or just explicitly manage it
    size_t threads = cores + pool_size + 4; 
    g_last_threads_count = (uint32_t)threads;
    g_user_thread_pool = tp_create(threads, (size_t)MAX_USER_CAPACITY);

    if (g_shm) {
        // Track: 1 (UM Main) + threads (Pool)
        atomic_fetch_add(&g_shm->stats.connected_threads, g_last_threads_count + 1);
        atomic_fetch_add(&g_shm->stats.active_threads, 1); // UM Main is active
        tp_set_active_counter(g_user_thread_pool, &g_shm->stats.active_threads);
    }
}

static void execute_user_simulation(void *arg) {
    user_task_ctx_t *ctx = (user_task_ctx_t *)arg;
    int slot_idx = ctx->slot; 

    // Categorize this thread's logs
    po_logger_set_thread_category(1); // User category

    atomic_fetch_add(&ctx->shm->stats.connected_users, 1);

    // Run simulation loop using the shared SHM mapping from the manager
    run_user_simulation_loop(ctx->user_id, ctx->service_type, ctx->shm, 
        &g_user_slots[slot_idx].should_continue_running);

    atomic_fetch_sub(&ctx->shm->stats.connected_users, 1);

    atomic_store(&g_user_slots[slot_idx].is_occupied, false);

    // Notify completion via waitgroup
    wg_done(g_user_wg);

    free(ctx);
}

int users_spawn_new(sim_shm_t *shm) {
    if (wg_active_count(g_user_wg) >= MAX_USER_CAPACITY) {
        LOG_WARN("Cannot spawn user: Max capacity (%d) reached.", MAX_USER_CAPACITY);
        return -1;
    }

    int slot = -1;
    for (int i = 0; i < MAX_USER_CAPACITY; i++) {
        bool expected = false;
        if (atomic_compare_exchange_strong(&g_user_slots[i].is_occupied, &expected, true)) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        LOG_WARN("Cannot spawn user: No free slots found (Internal mismatch?).");
        return -1;
    }

    atomic_store(&g_user_slots[slot].should_continue_running, true);

    user_task_ctx_t *ctx = malloc(sizeof(user_task_ctx_t));
    if (!ctx) {
        atomic_store(&g_user_slots[slot].is_occupied, false);
        return -1;
    }

    // Pseudorandom User ID
    ctx->user_id = (int)rand() + slot; 
    ctx->service_type = rand() % SIM_MAX_SERVICE_TYPES;
    ctx->slot = slot;  // Store actual slot
    ctx->shm = shm;

    // Track active user count via waitgroup
    wg_add(g_user_wg, 1);

    if (tp_submit(g_user_thread_pool, execute_user_simulation, ctx) != 0) {
        // Rollback if submission failed
        wg_done(g_user_wg);
        atomic_store(&g_user_slots[slot].is_occupied, false);
        free(ctx);
        return -1;
    }

    LOG_INFO("Spawned User %d in Slot %d (Population: %d)", ctx->user_id, slot, wg_active_count(g_user_wg));

    return 0;
}

int users_spawn_count(void) {
    return wg_active_count(g_user_wg);
}

void users_spawn_stop_random(void) {
    // Linear scan to find occupier - simple approach
    for (int i = MAX_USER_CAPACITY - 1; i >= 0; i--) {
        if (atomic_load(&g_user_slots[i].is_occupied)) {
            if (atomic_load(&g_user_slots[i].should_continue_running)) {
                atomic_store(&g_user_slots[i].should_continue_running, false);
                return; // Killed one
            }
        }
    }
}

void users_spawn_shutdown_all(void) {
    if (g_shm && g_last_threads_count > 0) {
        atomic_fetch_sub(&g_shm->stats.active_threads, 1);
        atomic_fetch_sub(&g_shm->stats.connected_threads, g_last_threads_count + 1);
    }
    for (int i = 0; i < MAX_USER_CAPACITY; i++) {
        atomic_store(&g_user_slots[i].should_continue_running, false);
    }
    tp_destroy(g_user_thread_pool, true);
    g_user_thread_pool = NULL;

    wg_destroy(g_user_wg);
    g_user_wg = NULL;
}
