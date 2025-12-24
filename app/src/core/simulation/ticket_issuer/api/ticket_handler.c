#define _POSIX_C_SOURCE 200809L
#include "ticket_handler.h"
#include <postoffice/net/net.h>
#include <postoffice/log/logger.h>
#include <postoffice/net/socket.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ipc/simulation_protocol.h"

void ticket_handler_process_request(int client_fd, sim_shm_t *shm) {
    LOG_DEBUG("Ticket handler invoked for client_fd=%d", client_fd);

    // Ensure socket is blocking for worker tasks
    po_socket_set_blocking(client_fd);

    po_header_t header;
    zcp_buffer_t *payload = NULL;

    int ret = net_recv_message_blocking(client_fd, &header, &payload);
    LOG_DEBUG("net_recv_message_blocking returned %d, payload=%p", ret, (void*)payload);

    if (ret == 0 && payload) {
        LOG_DEBUG("Received message type=0x%02X", header.msg_type);

        if (header.msg_type == MSG_TYPE_TICKET_REQ) {
            msg_ticket_req_t request;
            memcpy(&request, &((uint8_t*)payload)[0], sizeof(request));

            uint32_t ticket = atomic_fetch_add(&shm->ticket_seq, 1);
            LOG_DEBUG("Processed Request from PID %d -> Issued Ticket #%u", request.requester_pid, ticket);

            msg_ticket_resp_t resp = {
                .ticket_number = ticket,
                .assigned_service = request.service_type
            };

            int send_ret = net_send_message(client_fd, MSG_TYPE_TICKET_RESP, PO_FLAG_NONE, (uint8_t*)&resp, sizeof(resp));
            LOG_DEBUG("net_send_message returned %d", send_ret);
        } else {
            LOG_WARN("Unexpected message type: 0x%02X", header.msg_type);
        }
        net_zcp_release_rx(payload);
    } else {
        LOG_WARN("net_recv_message failed: ret=%d, payload=%p, errno=%d (%s)", 
            ret, (void*)payload, errno, strerror(errno));
    }

    LOG_DEBUG("Closing client_fd=%d", client_fd);
    po_socket_close(client_fd);
}
