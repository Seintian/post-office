#ifndef USERS_SPAWN_H
#define USERS_SPAWN_H

#include <postoffice/net/net.h>
#include <stdbool.h>

#include "ipc/simulation_ipc.h"

#define MAX_USER_CAPACITY 2000

typedef struct {
    atomic_bool is_occupied;
    atomic_bool should_continue_running;
    int worker_idx;
} user_slot_t;

void users_spawn_init(sim_shm_t *shm, size_t pool_size);

/**
 * @brief Spawns a new user thread.
 * @param shm Pointer to simulation shared memory.
 * @return 0 on success, -1 if no slots/resources.
 */
int users_spawn_new(sim_shm_t *shm);

/**
 * @brief Stops a random user by flagging it for shutdown.
 */
void users_spawn_stop_random(void);

int users_spawn_count(void);

/**
 * @brief Stops all user threads and waits for them to join.
 */
void users_spawn_shutdown_all(void);

#endif
