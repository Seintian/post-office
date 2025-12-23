/**
 * @file simulation_ipc.h
 * @brief Helper functions for Simulation IPC Lifecycle.
 * @ingroup simulation
 */

#ifndef PO_SIMULATION_IPC_H
#define PO_SIMULATION_IPC_H

#include "simulation_protocol.h"
#include <stddef.h>

/**
 * @brief Initialize (Create) the Simulation Shared Memory.
 * 
 * Creates the SHM object, truncates it to the size of sim_shm_t,
 * maps it into memory, and zero-initializes it.
 *
 * @param[in] n_workers Number of workers to allocate space for.
 * @return Pointer to mapped sim_shm_t on success, NULL on failure (errno set).
 * @note Thread-safe: No (Must be exclusive).
 */
sim_shm_t* sim_ipc_shm_create(size_t n_workers);

/**
 * @brief Attach to an existing Simulation Shared Memory.
 *
 * Maps the existing SHM object.
 *
 * @return Pointer to mapped sim_shm_t on success, NULL on failure (errno set).
 * @note Thread-safe: No (Initialization).
 */
sim_shm_t* sim_ipc_shm_attach(void);

/**
 * @brief Detach (unmap) the Shared Memory.
 *
 * @param[in] shm Pointer to the shared memory region.
 * @return 0 on success, -1 on failure.
 * @note Thread-safe: No.
 */
int sim_ipc_shm_detach(sim_shm_t* shm);

/**
 * @brief Destroy (unlink) the Shared Memory object from the system.
 * 
 * Should be called by the Director on shutdown.
 *
 * @return 0 on success, -1 on failure.
 * @note Thread-safe: No (Global destructor).
 */
int sim_ipc_shm_destroy(void);

/**
 * @brief Create global semaphores set.
 * 
 * @param[in] n_sems Number of semaphores to create.
 * @return Semaphore Set ID (semid) or -1 on failure.
 * @note Thread-safe: No.
 */
int sim_ipc_sem_create(int n_sems);

/**
 * @brief Get existing semaphores set.
 * 
 * @return Semaphore Set ID (semid) or -1 on failure.
 * @note Thread-safe: Yes (Read-only lookup).
 */
int sim_ipc_sem_get(void);

#endif // PO_SIMULATION_IPC_H
