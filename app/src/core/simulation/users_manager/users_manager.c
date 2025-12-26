/**
 * @file users_manager.c
 * @brief Manages the lifecycle of user processes.
 */

#define _POSIX_C_SOURCE 200809L

#include "spawn/users_spawn.h"
#include "ipc/sim_client.h"
#include "ipc/simulation_ipc.h"
#include "utils/signals.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>

#include <postoffice/log/logger.h>
#include <postoffice/net/net.h>

static int g_target_population = 0;
static volatile sig_atomic_t g_shutdown_requested = 0;
static void on_signal(int s, siginfo_t* i, void* c) {
    (void)s; (void)i; (void)c;
    g_shutdown_requested = 1;
}

static void on_scale_up(int s, siginfo_t* i, void* c) {
    (void)s; (void)i; (void)c;
    g_target_population += 10;
    LOG_INFO("Scale Up Signal: Target +10 -> %d", g_target_population);
}

static void on_scale_down(int s, siginfo_t* i, void* c) {
    (void)s; (void)i; (void)c;
    if (g_target_population >= 10) g_target_population -= 10;
    LOG_INFO("Scale Down Signal: Target -10 -> %d", g_target_population);
}

int main(int argc, char *argv[]) {
    // 1. Args
    int initial=5, batch=5, pool_size=64;
    char *loglevel = "INFO";
    int opt;
    struct option long_opts[] = {
        {"initial", required_argument, 0, 'i'},
        {"batch", required_argument, 0, 'b'},
        {"pool-size", required_argument, 0, 'p'},
        {"l", required_argument, 0, 'l'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "l:", long_opts, NULL)) != -1) {
        if (opt == 'i') initial = atoi(optarg);
        if (opt == 'b') batch = atoi(optarg);
        if (opt == 'p') pool_size = atoi(optarg);
        if (opt == 'l') loglevel = optarg;
    }

    g_target_population = initial;

    // 2. Logging
    po_logger_init(&(po_logger_config_t){
        .level = (po_logger_level_from_str(loglevel) == -1 ? LOG_INFO : po_logger_level_from_str(loglevel)),
        .ring_capacity = 8192,
        .consumers = 1
    });
    po_logger_add_sink_file_categorized("logs/users_manager.log", false, 1u << 0);
    po_logger_add_sink_file_categorized("logs/users.log", false, 1u << 1);
    // po_logger_add_sink_console(false);
    po_logger_set_thread_category(0); // Manager category

    // 3. SHM
    sim_shm_t *shm = sim_ipc_shm_attach();
    if (!shm) return 1;

    // 4. Client Init
    sim_client_setup_signals(on_signal);

    // Custom signals for scaling
    if (sigutil_handle(SIGUSR1, on_scale_up, 0) != 0 || 
        sigutil_handle(SIGUSR2, on_scale_down, 0) != 0) {
        LOG_FATAL("Failed to setup custom signals");
        return 1;
    }

    users_spawn_init((size_t)pool_size);

    if (net_init_zerocopy(128, 128, 4096) != 0) {
        LOG_FATAL("Users Manager: Failed to initialize net zerocopy");
        return 1;
    }

    LOG_INFO("Users Manager Started (Target=%d, Batch=%d)", initial, batch);

    int last_day = 0;

    // Pre-spawn initial users to avoid deadlock with Director (which waits for users before starting clock)
    if (g_target_population > 0) {
        LOG_INFO("Pre-Spawning initial %d users...", g_target_population);
        for (int k = 0; k < g_target_population; k++) {
            if (users_spawn_new(shm) != 0) {
                LOG_WARN("Failed to spawn initial user %d/%d", k + 1, g_target_population);
            }
        }
    }

    while (!g_shutdown_requested) {
        // Wait for Day Start
        sim_client_wait_barrier(shm, &last_day, &g_shutdown_requested);
        if (g_shutdown_requested) break;

        // Scale Population
        int current_pop = users_spawn_count();
        if (current_pop < g_target_population) {
            int needed = g_target_population - current_pop;
            if (needed > batch) needed = batch; // ramp up slowly

            LOG_INFO("Population Control: Spawning %d users (Current: %d, Target: %d)", needed, current_pop, g_target_population);

            for (int k=0; k<needed; k++) {
                if (users_spawn_new(shm) != 0) {
                    LOG_WARN("Failed to spawn user %d/%d in batch", k+1, needed);
                }
            }
        } else if (current_pop > g_target_population) {
            int remove = current_pop - g_target_population;

            LOG_INFO("Population Control: Reducing %d users (Current: %d, Target: %d)", remove, current_pop, g_target_population);

            for (int k=0; k<remove; k++) {
                users_spawn_stop_random();
            }
        }
        
        // Yield to prevent 100% CPU spin while waiting for next day/scaling
        usleep(100000); // 100ms
    }

    LOG_INFO("Users Manager Shutting Down...");
    users_spawn_shutdown_all();
    net_shutdown_zerocopy();
    sim_ipc_shm_detach(shm);
    po_logger_shutdown();
    return 0;
}
