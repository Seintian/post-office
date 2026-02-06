#ifndef BROKER_HANDLER_H
#define BROKER_HANDLER_H

#include "broker_core.h"

void broker_handler_process_request(int client_fd, broker_ctx_t *ctx);

#endif
