#ifndef USER_LOOP_H
#define USER_LOOP_H

#include <stdatomic.h>
#include <stdbool.h>

#include "../../ipc/simulation_protocol.h"

/**
 * @brief Initialize standalone user process runtime (logger, metrics, shm).
 * @param[out] out_shm Double pointer to receive the attached Shared Memory address.
 * @return 0 on success, non-zero on failure.
 */
int initialize_user_runtime(sim_shm_t **out_shm);

/**
 * @brief Teardown user process runtime.
 * @param[in] shm Pointer to shared memory to detach.
 */
void teardown_user_runtime(sim_shm_t *shm);

/**
 * @brief Executes the simulation loop for a single user agent.
 *
 * @param user_id The unique ID of the user.
 * @param service_type The service this user is seeking.
 * @param shm Pointer to shared memory.
 * @param should_continue_flag Atomic flag to control loop execution. May be NULL (loop runs unconditionally).
 *                             If non-NULL, flag is checked at loop boundaries and before blocking I/O operations.
 *                             Caller must ensure the flag is properly synchronized (atomic operations) when updating
 *                             from external threads; the flag is treated as a request to terminate gracefully.
 * @return 0 on success, non-zero on termination/error.
 */
int run_user_simulation_loop(int user_id, int service_type, sim_shm_t *shm,
                             volatile atomic_bool *should_continue_flag);

#endif
