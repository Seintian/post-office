#define _POSIX_C_SOURCE 200809L
#include "broker_handler.h"

#include <postoffice/log/logger.h>
#include <postoffice/net/net.h>
#include <postoffice/net/socket.h>
#include <postoffice/priority_queue/priority_queue.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ipc/simulation_ipc.h"
#include "ipc/simulation_protocol.h"

// --- Broker Item Structure ---
typedef struct {
    uint32_t ticket_number;
    int is_vip;
    pid_t requester_pid;
    struct timespec arrival_time;
} broker_item_t;

// --- External Globals (defined in work_broker.c) ---
extern po_priority_queue_t *g_queues[SIM_MAX_SERVICE_TYPES];
extern pthread_mutex_t g_queue_mutexes[SIM_MAX_SERVICE_TYPES];

void broker_handler_process_request(int client_fd, sim_shm_t *shm) {
    LOG_DEBUG("Broker: Handler invoked for client_fd=%d", client_fd);

    po_socket_set_blocking(client_fd);

    po_header_t header;
    zcp_buffer_t *payload = NULL;

    int ret = net_recv_message_blocking(client_fd, &header, &payload);

    if (ret != 0 || !payload) {
        LOG_WARN("Broker: Failed to recv message (ret=%d)", ret);
        po_socket_close(client_fd);
        if (payload)
            net_zcp_release_rx(payload);
        return;
    }

    if (header.msg_type == MSG_TYPE_JOIN_QUEUE) {
        msg_join_queue_t req;
        memcpy(&req, payload, sizeof(req));
        net_zcp_release_rx(payload);

        if (req.service_type >= SIM_MAX_SERVICE_TYPES) {
            LOG_ERROR("Broker: Invalid service type %d", req.service_type);
            po_socket_close(client_fd);
            return;
        }

        // 1. Issue Ticket
        uint32_t ticket = atomic_fetch_add(&shm->ticket_seq, 1);

        // 2. Add to Priority Queue
        broker_item_t *item = malloc(sizeof(broker_item_t));
        if (item) {
            item->ticket_number = ticket;
            item->is_vip = req.is_vip;
            item->requester_pid = req.requester_pid;
            clock_gettime(CLOCK_MONOTONIC, &item->arrival_time);

            pthread_mutex_lock(&g_queue_mutexes[req.service_type]);
            if (po_priority_queue_push(g_queues[req.service_type], item) != 0) {
                LOG_ERROR("Broker: Failed to push to queue %d", req.service_type);
                free(item);
            } else {
                LOG_DEBUG("Broker: Enqueued Ticket %u (VIP=%d) for Service %d", ticket, req.is_vip,
                          req.service_type);
            }
            pthread_mutex_unlock(&g_queue_mutexes[req.service_type]);
        }

        // 3. Send Ack
        msg_join_ack_t resp = {.ticket_number = ticket, .estimated_wait_ms = 0};
        net_send_message(client_fd, MSG_TYPE_JOIN_ACK, PO_FLAG_NONE, (uint8_t *)&resp,
                         sizeof(resp));

    } else if (header.msg_type == MSG_TYPE_GET_WORK) {
        msg_get_work_t req;
        memcpy(&req, payload, sizeof(req));
        net_zcp_release_rx(payload);

        if (req.service_type >= SIM_MAX_SERVICE_TYPES) {
            po_socket_close(client_fd);
            return;
        }

        // 1. Pop from Priority Queue
        pthread_mutex_lock(&g_queue_mutexes[req.service_type]);
        broker_item_t *item = po_priority_queue_pop(g_queues[req.service_type]);
        pthread_mutex_unlock(&g_queue_mutexes[req.service_type]);

        msg_work_item_t resp = {0};
        if (item) {
            resp.ticket_number = item->ticket_number;
            resp.is_vip = item->is_vip;
            free(item); // Processed
            LOG_DEBUG("Broker: Assigned Ticket %u to Worker (PID %d)", resp.ticket_number,
                      req.worker_pid);
        } else {
            resp.ticket_number = 0; // No work available
        }

        // 2. Send Work Item
        net_send_message(client_fd, MSG_TYPE_WORK_ITEM, PO_FLAG_NONE, (uint8_t *)&resp,
                         sizeof(resp));

    } else {
        LOG_WARN("Broker: Unexpected message type 0x%02X", header.msg_type);
        net_zcp_release_rx(payload);
    }

    po_socket_close(client_fd);
}
