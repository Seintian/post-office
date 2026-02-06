/**
 * @file director.c
 * @brief Orchestrates the entire simulation context.
 */

#define _POSIX_C_SOURCE 200809L

#include <postoffice/sysinfo/sysinfo.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "director_cleanup.h"
#include "director_config.h"
#include "director_orch.h"
#include "director_setup.h"
#include "director_time.h"

int main(int argc, char *argv[]) {
    // 1. Config
    director_config_t cfg;
    initialize_configuration_defaults(&cfg);
    parse_command_line_configuration(&cfg, argc, argv);

    // 2. Setup Subsystems & Signals
    volatile sig_atomic_t running = 1;
    volatile sig_atomic_t sigchld_received = 0;

    director_sig_ctx_t sig_ctx = {.running_flag = &running, .sigchld_flag = &sigchld_received};

    if (director_setup_subsystems(&cfg, sig_ctx) != 0) {
        return 1;
    }

    // 3. System Info & Shared Memory
    po_sysinfo_t sysinfo;
    po_sysinfo_collect(&sysinfo);

    sim_shm_t *shm = director_setup_shm(&cfg, &sysinfo);
    if (!shm) {
        // Logging is initialized so we can log fatal here if needed, but setup_shm likely logged
        // error
        director_cleanup(&cfg); // Clean up loggers etc
        return 1;
    }

    // 4. Launch
    spawn_simulation_subsystems(&cfg);

    // 5. Clock Loop
    execute_simulation_clock_loop(shm, &cfg, &running, &sigchld_received, cfg.initial_users);

    // 6. Shutdown
    if (shm) {
        atomic_fetch_sub(&shm->stats.active_threads, 1);
    }

    director_cleanup(&cfg);
    return 0;
}
