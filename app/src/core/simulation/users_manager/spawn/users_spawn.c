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

static user_slot_t g_user_slots[MAX_USER_CAPACITY];
static volatile _Atomic int g_current_population_count = 0;
static threadpool_t *g_user_thread_pool = NULL;

typedef struct {
    int user_id;
    int service_type;
    int slot;  // Actual slot index, not recalculated
    sim_shm_t *shm;
} user_task_ctx_t;

void users_spawn_init(size_t pool_size) {
    LOG_INFO("Initializing User Thread Pool (PoolSize: %zu, MaxCapacity: %d)", pool_size, MAX_USER_CAPACITY);
    for (int i = 0; i < MAX_USER_CAPACITY; i++) {
        atomic_init(&g_user_slots[i].is_occupied, false);
        atomic_init(&g_user_slots[i].should_continue_running, false);
    }
    atomic_init(&g_current_population_count, 0);
    g_user_thread_pool = tp_create((size_t)sysconf(_SC_NPROCESSORS_ONLN) + pool_size + 4, MAX_USER_CAPACITY);
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
    atomic_fetch_sub(&g_current_population_count, 1);
    free(ctx);
}

int users_spawn_new(sim_shm_t *shm) {
    if (atomic_load(&g_current_population_count) >= MAX_USER_CAPACITY) {
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

    tp_submit(g_user_thread_pool, execute_user_simulation, ctx);
    atomic_fetch_add(&g_current_population_count, 1);
    
    LOG_INFO("Spawned User %d in Slot %d (Population: %d)", ctx->user_id, slot, atomic_load(&g_current_population_count));

    return 0;
}

int users_spawn_count(void) {
    return atomic_load(&g_current_population_count);
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
    for (int i = 0; i < MAX_USER_CAPACITY; i++) {
        atomic_store(&g_user_slots[i].should_continue_running, false);
    }
    tp_destroy(g_user_thread_pool, true);
    g_user_thread_pool = NULL;
}
