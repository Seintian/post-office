#ifndef USER_LOOP_H
#define USER_LOOP_H

#include <stdatomic.h>
#include <stdbool.h>
#include "../../ipc/simulation_protocol.h"

/**
 * Initialize standalone user process resources (logger, metrics, shm).
 */
int user_standalone_init(sim_shm_t **out_shm);

/**
 * Cleanup standalone user process resources.
 */
void user_standalone_cleanup(sim_shm_t *shm);

/**
 * Run the user loop.
 * @param user_id Unique ID for this user.
 * @param service_type Service type the user needs.
 * @param shm Pointer to attached shared memory.
 * @param active_flag Optional flag to monitor for cancellation.
 */
int user_run(int user_id, int service_type, sim_shm_t *shm, volatile _Atomic bool *active_flag);

#endif
