#ifndef DIRECTOR_CONFIG_H
#define DIRECTOR_CONFIG_H

#include <postoffice/sysinfo/sysinfo.h>
#include <stdint.h>

#include "ipc/simulation_ipc.h"

// Parsed Config State
typedef struct {
    uint32_t worker_count;
    char *config_path;
    const char *log_level;
    bool is_headless;

    // Process Configs
    int issuer_pool_size;
    int manager_pool_size;
    int initial_users;
    int batch_users;

    // Load Balancing
    bool lb_enabled;
    uint32_t lb_check_interval;      // Check frequency (sim minutes)
    uint32_t lb_imbalance_threshold; // Trigger ratio (e.g., 200 = 2x)
    uint32_t lb_min_queue_depth;     // Ignore near-empty queues
} director_config_t;

void initialize_configuration_defaults(director_config_t *cfg);
void parse_command_line_configuration(director_config_t *cfg, int argc, char **argv);
void resolve_complete_configuration(director_config_t *cfg, po_sysinfo_t *sysinfo);
void apply_configuration_to_shared_memory(director_config_t *cfg, sim_shm_t *shm);

#endif
