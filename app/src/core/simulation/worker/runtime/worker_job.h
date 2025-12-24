#ifndef WORKER_JOB_H
#define WORKER_JOB_H

#include <stdint.h>
#include "ipc/simulation_ipc.h"

void worker_job_simulate(int worker_id, int service_type, uint32_t ticket, sim_shm_t *shm);

#endif
