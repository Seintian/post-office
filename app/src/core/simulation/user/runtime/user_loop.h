#ifndef USER_LOOP_H
#define USER_LOOP_H

#include "../../ipc/simulation_ipc.h"

#include <stdbool.h>
#include <stdatomic.h>

/**
 * Run the user logic loop.
 * @param user_id User ID.
 * @param service_type Service type.
 * @param shm Pointer to attached shared memory.
 * @param active_flag Optional cancellation flag (thread pool usage).
 * @return 0 on success.
 */
int user_run(int user_id, int service_type, sim_shm_t* shm, volatile _Atomic bool *active_flag);

/**
 * Initialize resources for standalone user process (logger, SHM, signals).
 * @param[out] out_shm Receives pointer to attached SHM.
 * @return 0 on success.
 */
int user_standalone_init(sim_shm_t** out_shm);

#endif