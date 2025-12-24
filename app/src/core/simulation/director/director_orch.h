#ifndef DIRECTOR_ORCH_H
#define DIRECTOR_ORCH_H

#include <sys/types.h>
#include "director_config.h"

void initialize_process_orchestrator(void);

// Launches all child processes based on config
void spawn_simulation_subsystems(const director_config_t *cfg);

// Graceful shutdown
void terminate_all_simulation_subsystems(void);

#endif
