#ifndef SIMULATION_LIFECYCLE_H
#define SIMULATION_LIFECYCLE_H

#include <stdbool.h>

/**
 * Global configuration path for the simulation.
 * Set during initialization and accessible throughout the application.
 */
extern const char* g_simulation_config_path;

/**
 * @brief Initialize the simulation lifecycle subsystem.
 * @param[in] config_path Path to the configuration file (optional, may be NULL).
 * @note Thread-safe: No (called from main).
 */
void simulation_init(const char* config_path);

/**
 * @brief Start the simulation processes (Director, etc.).
 * Does not block.
 * @param[in] tui_mode If true, suppress stdout of child processes.
 * @param[in] loglevel Log level to pass to the Director.
 * @note Thread-safe: No.
 */
void simulation_start(bool tui_mode, int loglevel);

/**
 * @brief Stop the simulation processes.
 * Uses SIGTERM and waits for Director to exit.
 * @note Thread-safe: No.
 */
void simulation_stop(void);

/**
 * @brief Run the simulation in headless mode (blocks until signal).
 * Handles signal trapping and cleanup.
 * @note Thread-safe: No.
 */
void simulation_run_headless(void);

#endif // SIMULATION_LIFECYCLE_H
