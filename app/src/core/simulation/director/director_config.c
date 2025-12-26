#define _POSIX_C_SOURCE 200809L
#include "director_config.h"
#include "utils/configs.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <postoffice/log/logger.h>


void initialize_configuration_defaults(director_config_t *cfg) {
    if (!cfg) return;
    cfg->worker_count = 0;
    cfg->config_path = NULL;
    cfg->log_level = "INFO";
    cfg->is_headless = false;
    cfg->issuer_pool_size = 64;
    cfg->manager_pool_size = 1000;
    cfg->initial_users = 5;
    cfg->batch_users = 5;
}

void parse_command_line_configuration(director_config_t *cfg, int argc, char **argv) {
    static struct option long_opts[] = {
        {"headless", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'c'},
        {"loglevel", required_argument, 0, 'l'},
        {"workers", required_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hc:l:w:", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'h':
            cfg->is_headless = true;
            break;
        case 'c':
            cfg->config_path = optarg;
            break;
        case 'l':
            if (po_logger_level_from_str(optarg) != -1) {
                cfg->log_level = optarg;
            }
            break;
        case 'w': {
            long val = strtol(optarg, NULL, 10);
            if (val > 0) cfg->worker_count = (uint32_t)val;
            break;
        }
        }
    }
}

void resolve_complete_configuration(director_config_t *cfg, po_sysinfo_t *sysinfo) {
    // Load configuration file if provided
    if (cfg->config_path) {
        po_config_t *file_cfg = NULL;
        if (po_config_load_strict(cfg->config_path, &file_cfg) == 0) {
            int workers;
            if (po_config_get_int(file_cfg, "workers", "NOF_WORKERS", &workers) == 0 && workers > 0) {
                if (cfg->worker_count == 0) cfg->worker_count = (uint32_t)workers;
            }

            int batch;
            if (po_config_get_int(file_cfg, "users_manager", "N_NEW_USERS", &batch) == 0) {
                cfg->batch_users = batch;
                cfg->initial_users = batch; 
            }

            int um_pool;
            if (po_config_get_int(file_cfg, "users_manager", "POOL_SIZE", &um_pool) == 0)
                cfg->manager_pool_size = um_pool;

            int init_users;
            if (po_config_get_int(file_cfg, "users", "NOF_USERS", &init_users) == 0)
                cfg->initial_users = init_users;

            int reqs;
            if (po_config_get_int(file_cfg, "users", "N_REQUESTS", &reqs) == 0) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", reqs);
                setenv("PO_USER_REQUESTS", buf, 1);
            }

            int ti_pool;
            if (po_config_get_int(file_cfg, "ticket_issuer", "POOL_SIZE", &ti_pool) == 0)
                cfg->issuer_pool_size = ti_pool;

            po_config_free(&file_cfg);
        } else {
            LOG_ERROR("Failed to load config file: %s", cfg->config_path);
        }
    }

    // Auto-detect workers if not set
    if (cfg->worker_count == 0) {
        if (sysinfo->logical_processors > 0) {
            cfg->worker_count = (uint32_t)sysinfo->logical_processors;
            if (cfg->worker_count < 2) cfg->worker_count = 2;
        } else {
            cfg->worker_count = DEFAULT_WORKERS;
        }
    }

    LOG_INFO("Configuration Resolved: Workers=%u, Initial Users=%d, Batch Users=%d", 
        cfg->worker_count, cfg->initial_users, cfg->batch_users);
}

void apply_configuration_to_shared_memory(director_config_t *cfg, sim_shm_t *shm) {
    // Set simulation parameters
    shm->params.sim_duration_days = 10;
    shm->params.tick_nanos = 2500000;
    shm->params.explode_threshold = 100;

    // Load config file for simulation-specific parameters
    if (cfg->config_path) {
        po_config_t *file_cfg = NULL;
        if (po_config_load_strict(cfg->config_path, &file_cfg) == 0) {
            int duration;
            if (po_config_get_int(file_cfg, "simulation", "SIM_DURATION", &duration) == 0)
                shm->params.sim_duration_days = (uint32_t)duration;

            long tick_ns;
            if (po_config_get_long(file_cfg, "simulation", "N_NANO_SECS", &tick_ns) == 0)
                shm->params.tick_nanos = (uint64_t)tick_ns;

            int explode;
            if (po_config_get_int(file_cfg, "simulation", "EXPLODE_THRESHOLD", &explode) == 0)
                shm->params.explode_threshold = (uint32_t)explode;

            po_config_free(&file_cfg);
        }
    }

    // Write worker count (already resolved)
    shm->params.n_workers = cfg->worker_count;
    
    // Barrier synchronization: 1 Worker Process + Users Manager + Ticket Issuer
    // We treat the Worker Process as a single participant representing all threads.
    atomic_store(&shm->sync.required_count, 1 + 2);

    LOG_INFO("Config Applied to SHM: Workers=%u, (Sync Req=%u), Duration=%u days", 
        cfg->worker_count, 3, shm->params.sim_duration_days);
}
