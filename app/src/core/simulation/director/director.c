/**
 * @file director.c
 * @brief Orchestrates the entire simulation context.
 */

#define _POSIX_C_SOURCE 200809L

#include <postoffice/log/logger.h>
#include <postoffice/net/socket.h>
#include <postoffice/sort/sort.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <stdio.h>
#include <stdlib.h>

#include "ctrl_bridge/bridge_mainloop.h"
#include "director_config.h"
#include "director_orch.h"
#include "director_time.h"
#include "ipc/simulation_ipc.h"
#include "utils/signals.h"

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_sigchld_received = 0;

static void handle_sigchld(int sig, siginfo_t *info, void *ctx) {
    (void)sig;
    (void)info;
    (void)ctx;
    g_sigchld_received = 1;
}

static void handle_signal(int sig, siginfo_t *info, void *ctx) {
    (void)sig;
    (void)info;
    (void)ctx;
    g_running = 0;
}

int main(int argc, char *argv[]) {
    // 1. Config
    director_config_t cfg;
    initialize_configuration_defaults(&cfg);
    parse_command_line_configuration(&cfg, argc, argv);
    po_sort_init();

    // 2. Logging
    if (po_logger_init(&(po_logger_config_t){.level = po_logger_level_from_str(cfg.log_level) == -1
                                                          ? LOG_INFO
                                                          : po_logger_level_from_str(cfg.log_level),
                                             .ring_capacity = 4096,
                                             .consumers = 1}) != 0)
        return 1;
    po_logger_add_sink_file("logs/director.log", false);
    po_logger_add_sink_console(false);

    // 3. System Info
    po_sysinfo_t sysinfo;
    po_sysinfo_collect(&sysinfo);

    // 4. Config Finalization & SHM Creation
    resolve_complete_configuration(&cfg, &sysinfo);

    sim_shm_t *shm = sim_ipc_shm_create(cfg.worker_count);
    if (!shm)
        return 1;

    apply_configuration_to_shared_memory(&cfg, shm);

    if (!cfg.is_headless) {
        bridge_mainloop_init(shm);
    }

    // 5. Signals
    if (sigutil_setup(handle_signal, SIGUTIL_HANDLE_TERMINATING_ONLY, 0) != 0) {
        LOG_FATAL("Failed to setup signals");
        return 1;
    }
    // Handle status change of children (Crashes)
    if (sigutil_handle(SIGCHLD, handle_sigchld, 0) != 0) {
        LOG_FATAL("Failed to setup SIGCHLD handler");
        return 1;
    }

    // 6. Launch
    initialize_process_orchestrator();
    spawn_simulation_subsystems(&cfg);

    // 7. Clock Loop
    execute_simulation_clock_loop(shm, &g_running, &g_sigchld_received, cfg.initial_users);

    // 8. Shutdown
    LOG_INFO("Director shutting down...");
    if (shm) {
        atomic_fetch_sub(&shm->stats.active_threads, 1);
    }
    terminate_all_simulation_subsystems();

    if (!cfg.is_headless) {
        bridge_mainloop_stop();
    }
    sim_ipc_shm_destroy();
    po_sort_finish();
    po_logger_shutdown();
    return 0;
}
