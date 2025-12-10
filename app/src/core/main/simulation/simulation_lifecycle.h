#ifndef SIMULATION_LIFECYCLE_H
#define SIMULATION_LIFECYCLE_H

/**
 * Global configuration path for the simulation.
 * Set during initialization and accessible throughout the application.
 */
extern const char* g_simulation_config_path;

/**
 * Initialize the simulation lifecycle subsystem.
 * @param config_path Path to the configuration file.
 */
void simulation_init(const char* config_path);

/**
 * Start the simulation processes (Director, etc.).
 * Does not block.
 */
void simulation_start(void);

/**
 * Stop the simulation processes.
 */
void simulation_stop(void);

/**
 * Run the simulation in headless mode (blocks until signal).
 * Handles signal trapping and cleanup.
 */
void simulation_run_headless(void);

#endif // SIMULATION_LIFECYCLE_H
