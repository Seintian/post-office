#define _POSIX_C_SOURCE 200809L

#include "director_setup.h"

#include <postoffice/log/logger.h>
#include <postoffice/sort/sort.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <stdio.h>
#include <string.h>

#include "ctrl_bridge/bridge_mainloop.h"
#include "director_orch.h"
#include "utils/signals.h"

// Static pointers for signal handlers to access flags in main
static volatile sig_atomic_t *g_ptr_running = NULL;
static volatile sig_atomic_t *g_ptr_sigchld = NULL;

static void handle_sigchld(int sig, siginfo_t *info, void *ctx) {
    (void)sig;
    (void)info;
    (void)ctx;
    if (g_ptr_sigchld)
        *g_ptr_sigchld = 1;
}

static void handle_signal(int sig, siginfo_t *info, void *ctx) {
    (void)sig;
    (void)info;
    (void)ctx;
    if (g_ptr_running)
        *g_ptr_running = 0;
}

int director_setup_subsystems(const director_config_t *cfg, director_sig_ctx_t sig_ctx) {
    // 1. Sort System
    po_sort_init();

    // 2. Logging
    po_logger_config_t log_cfg = {.level = po_logger_level_from_str(cfg->log_level) == -1
                                               ? LOG_INFO
                                               : po_logger_level_from_str(cfg->log_level),
                                  .ring_capacity = 4096,
                                  .consumers = 1};

    if (po_logger_init(&log_cfg) != 0) {
        return -1;
    }
    po_logger_add_sink_file("logs/director.log", false);
    po_logger_add_sink_console(false);

    // 3. Signals
    g_ptr_running = sig_ctx.running_flag;
    g_ptr_sigchld = sig_ctx.sigchld_flag;

    if (sigutil_setup(handle_signal, SIGUTIL_HANDLE_TERMINATING_ONLY, 0) != 0) {
        LOG_FATAL("Failed to setup signals");
        return -1;
    }
    if (sigutil_handle(SIGCHLD, handle_sigchld, 0) != 0) {
        LOG_FATAL("Failed to setup SIGCHLD handler");
        return -1;
    }

    // 4. Orchestrator
    initialize_process_orchestrator();

    return 0;
}

sim_shm_t *director_setup_shm(director_config_t *cfg, po_sysinfo_t *sysinfo) {
    // Resolve completes config based on sysinfo
    resolve_complete_configuration(cfg, sysinfo);

    sim_shm_t *shm = sim_ipc_shm_create(cfg->worker_count);
    if (!shm)
        return NULL;

    apply_configuration_to_shared_memory(cfg, shm);

    if (!cfg->is_headless) {
        bridge_mainloop_init(shm);
    }

    return shm;
}
