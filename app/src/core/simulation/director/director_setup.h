#ifndef DIRECTOR_SETUP_H
#define DIRECTOR_SETUP_H

#include <signal.h>
#include <stdbool.h>

#include "director_config.h"
#include "ipc/simulation_ipc.h"

// Forward declaration for signal flags
typedef struct {
    volatile sig_atomic_t *running_flag;
    volatile sig_atomic_t *sigchld_flag;
} director_sig_ctx_t;

/**
 * @brief Initialize all director subsystems (Sort, Logger, Signals).
 * @param cfg Configuration structure.
 * @param sig_ctx Context for signal handling (out-params for flags).
 * @return 0 on success, non-zero on error.
 */
int director_setup_subsystems(const director_config_t *cfg, director_sig_ctx_t sig_ctx);

/**
 * @brief Initialize Shared Memory and Simulation Context.
 * @param cfg Configuration structure.
 * @param sysinfo System info structure.
 * @return Pointer to attached SHM or NULL on error.
 */
sim_shm_t *director_setup_shm(director_config_t *cfg, po_sysinfo_t *sysinfo);

#endif
