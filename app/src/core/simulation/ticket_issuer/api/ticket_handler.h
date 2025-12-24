#ifndef TICKET_HANDLER_H
#define TICKET_HANDLER_H

#include "ipc/simulation_ipc.h"

void ticket_handler_process_request(int client_fd, sim_shm_t *shm);

#endif
