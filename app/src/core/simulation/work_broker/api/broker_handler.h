#ifndef BROKER_HANDLER_H
#define BROKER_HANDLER_H

#include "ipc/simulation_protocol.h"

void broker_handler_process_request(int client_fd, sim_shm_t *shm);

#endif
