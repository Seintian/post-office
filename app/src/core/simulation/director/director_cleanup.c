#define _POSIX_C_SOURCE 200809L

#include "director_cleanup.h"

#include <postoffice/log/logger.h>
#include <postoffice/sort/sort.h>
#include <stdatomic.h>
#include <stddef.h>

#include "ctrl_bridge/bridge_mainloop.h"
#include "director_orch.h"
#include "ipc/simulation_ipc.h"

void director_cleanup(const director_config_t *cfg) {
    // 1. Terminate Subsystems
    LOG_INFO("Director shutting down...");
    terminate_all_simulation_subsystems();

    // 2. Bridge
    if (!cfg->is_headless) {
        bridge_mainloop_stop();
    }

    // 3. SHM (We don't have the pointer here, but sim_ipc_shm_destroy handles global cleanup if
    // designed so, or we assume the caller handles the specific SHM pointer cleanup if needed.
    // Checking director.c: it calls sim_ipc_shm_destroy())
    sim_ipc_shm_destroy();

    // 4. Subsystems
    po_sort_finish();
    po_logger_shutdown();
}
