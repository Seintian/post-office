#ifndef DIRECTOR_TIME_H
#define DIRECTOR_TIME_H

#include "ipc/simulation_ipc.h"
#include <signal.h>

// Coordinate the day start barrier
// Coordinate the day start barrier
void synchronize_simulation_barrier(sim_shm_t *shm, int day, volatile sig_atomic_t *running_flag);

// Main clock loop
void execute_simulation_clock_loop(sim_shm_t *shm, volatile sig_atomic_t *running_flag, int expected_users);

#endif
