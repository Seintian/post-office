/**
 * @file work_broker.c
 * @brief Work Broker Process Implementation.
 * Replaces Ticket Issuer. Manages Priority Queues for User Requests.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <postoffice/concurrency/threadpool.h>
#include <postoffice/log/logger.h>
#include <postoffice/net/net.h>
#include <postoffice/net/poller.h>
#include <postoffice/net/socket.h>
#include <postoffice/priority_queue/priority_queue.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utils/signals.h>

#include "ipc/sim_client.h"
#include "ipc/simulation_ipc.h"
#include "ipc/simulation_protocol.h"

// --- Constants & Types ---

typedef struct {
    uint32_t ticket_number;
    int is_vip;
    pid_t requester_pid;
    struct timespec arrival_time;
} broker_item_t;

// Context for worker thread
typedef struct {
    int client_fd;
    sim_shm_t *shm;
} task_ctx_t;

// Globals
static volatile sig_atomic_t g_shutdown = 0;
static po_priority_queue_t *g_queues[SIM_MAX_SERVICE_TYPES];
static pthread_mutex_t g_queue_mutexes[SIM_MAX_SERVICE_TYPES];

// --- Helpers ---

// Hash function for broker_item_t pointer (identity hash)
static unsigned long item_hash(const void *ptr) {
    return (unsigned long)(uintptr_t)ptr;
}

// Comparator: Returns < 0 if a < b (Higher Priority).
static int item_compare(const void *a, const void *b) {
    const broker_item_t *ia = (const broker_item_t *)a;
    const broker_item_t *ib = (const broker_item_t *)b;

    // 1. VIP Status (Higher is better)
    if (ia->is_vip != ib->is_vip) {
        return ib->is_vip - ia->is_vip;
    }

    // 2. Arrival Time (Lower/Older is better -> FIFO within priority)
    if (ia->arrival_time.tv_sec != ib->arrival_time.tv_sec) {
        return (int)(ia->arrival_time.tv_sec - ib->arrival_time.tv_sec);
    }
    return (int)(ia->arrival_time.tv_nsec - ib->arrival_time.tv_nsec);
}

static void on_sig(int s, siginfo_t *i, void *c) {
    (void)s;
    (void)i;
    (void)c;
    g_shutdown = 1;
}

// --- Request Handler ---

static void handle_request(int client_fd, sim_shm_t *shm) {
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
                // Should send error back?
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
            // No work
            resp.ticket_number = 0;
        }

        // 2. Send Work Item
        net_send_message(client_fd, MSG_TYPE_WORK_ITEM, PO_FLAG_NONE, (uint8_t *)&resp,
                         sizeof(resp));

    } else {
        // Unknown or Legacy Ticket Req?
        if (payload)
            net_zcp_release_rx(payload); // Ensure released if not handled
        LOG_WARN("Broker: Unexpected message type 0x%02X", header.msg_type);
    }

    po_socket_close(client_fd);
}

static void worker_task(void *arg) {
    task_ctx_t *ctx = (task_ctx_t *)arg;
    handle_request(ctx->client_fd, ctx->shm);
    free(ctx);
}

// --- Main ---

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    po_sysinfo_t sysinfo;
    po_sysinfo_collect(&sysinfo);

    // 1. Args & Logging (Simplified for bevity)
    if (po_logger_init(
            &(po_logger_config_t){.level = LOG_DEBUG, .ring_capacity = 4096, .consumers = 1}) != 0)
        return 1;
    po_logger_add_sink_file("logs/work_broker.log", true);

    // 2. Init Queues
    for (int i = 0; i < SIM_MAX_SERVICE_TYPES; i++) {
        g_queues[i] = po_priority_queue_create(item_compare, item_hash);
        pthread_mutex_init(&g_queue_mutexes[i], NULL);
        if (!g_queues[i])
            LOG_FATAL("Failed to create queue %d", i);
    }

    // 3. Connect SHM
    sim_shm_t *shm = sim_ipc_shm_attach();
    if (!shm)
        return 1;

    sim_client_setup_signals(on_sig);

    if (net_init_zerocopy(128, 128, 4096) != 0)
        return 1;

    // 4. Listen
    const char *user_home = getenv("HOME");
    char sock_path[1024];
    snprintf(sock_path, sizeof(sock_path), "%s/.postoffice/issuer.sock",
             user_home ? user_home : "/tmp");

    unlink(sock_path);
    int fd = po_socket_listen_unix(sock_path, 128);
    if (fd < 0) {
        LOG_FATAL("Bind failed: %s", sock_path);
        return 1;
    }
    LOG_INFO("Work Broker Listening on %s", sock_path);

    // 5. Thread Pool
    threadpool_t *tp = tp_create(16, 4096);
    atomic_fetch_add(&shm->stats.connected_threads, 17);
    atomic_fetch_add(&shm->stats.active_threads, 1);
    tp_set_active_counter(tp, &shm->stats.active_threads);

    poller_t *poller = poller_create();
    poller_add(poller, fd, EPOLLIN);

    struct epoll_event ev[32];
    int last_day = 0;

    while (!g_shutdown) {
        int n = poller_wait(poller, ev, 32, 100);
        if (n < 0 && errno != EINTR)
            break;

        for (int i = 0; i < n; i++) {
            if (ev[i].data.fd == fd) {
                while (!g_shutdown) {
                    int client = po_socket_accept(fd, NULL, 0);
                    if (client < 0)
                        break;

                    task_ctx_t *ctx = malloc(sizeof(task_ctx_t));
                    ctx->client_fd = client;
                    ctx->shm = shm;
                    tp_submit(tp, worker_task, ctx);
                }
            }
        }

        if (atomic_load(&shm->sync.barrier_active)) {
            sim_client_wait_barrier(shm, &last_day, &g_shutdown);
        }
    }

    LOG_INFO("Broker Shutdown");

    // Cleanup
    tp_destroy(tp, true);
    poller_destroy(poller);
    net_shutdown_zerocopy();
    sim_ipc_shm_detach(shm);

    for (int i = 0; i < SIM_MAX_SERVICE_TYPES; i++) {
        po_priority_queue_destroy(g_queues[i]);
        pthread_mutex_destroy(&g_queue_mutexes[i]);
    }
    po_logger_shutdown();
    return 0;
}
