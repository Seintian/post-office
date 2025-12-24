#ifndef WORKER_LOOP_H
#define WORKER_LOOP_H
#include "ipc/simulation_ipc.h"

/**
 * @brief Initialize global runtime environment for the worker process.
 * This includes setting up logging, metrics, shared memory, and signal handling.
 * 
 * @return Pointer to attached SHM on success, NULL on error.
 */
sim_shm_t* initialize_worker_runtime(void);

/**
 * @brief Teardown global runtime environment.
 * Cleans up shared memory attachments and logs.
 */
void teardown_worker_runtime(sim_shm_t *shm);

/**
 * @brief Executes the main service loop for a worker.
 * 
 * @param worker_id The unique identifier for this worker instance (0 to N-1).
 * @param service_type The type of service this worker provides.
 * @param shm Pointer to shm.
 * @return EXIT_SUCCESS or EXIT_FAILURE.
 */
int run_worker_service_loop(int worker_id, int service_type, sim_shm_t *shm);

#endif