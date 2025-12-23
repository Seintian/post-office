/**
 * @file ticket_issuer.c
 * @brief Ticket Issuer Process Implementation.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <postoffice/concurrency/threadpool.h>
#include <postoffice/log/logger.h>
#include <postoffice/net/net.h>
#include <postoffice/net/poller.h>
#include <postoffice/net/socket.h>
#include <postoffice/perf/cache.h>
#include <postoffice/perf/perf.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utils/errors.h>
#include <utils/signals.h>

#include "../ipc/simulation_ipc.h"
#include "../ipc/simulation_protocol.h"

static volatile sig_atomic_t g_running = 1;

/**
 * @brief Signal handler for graceful shutdown.
 * @param[in] sig Signal number.
 * @param[in] info Signal info.
 * @param[in] context User context.
 */
static void handle_signal(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)info;
    (void)context;
    g_running = 0;
}

/**
 * @brief Register signal handlers using sigutil.
 */
static void setup_signals(void) {
    if (sigutil_handle_terminating(handle_signal, 0) != 0) {
        LOG_WARN("Failed to register signal handlers");
    }
}

/**
 * @brief Extract current simulation time from SHM.
 * @param[in] shm Shared memory pointer.
 * @param[out] d Day.
 * @param[out] h Hour.
 * @param[out] m Minute.
 */
static void get_sim_time(sim_shm_t *shm, int *d, int *h, int *m) {
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *d = (packed >> 16) & 0xFFFF;
    *h = (packed >> 8) & 0xFF;
    *m = packed & 0xFF;
}

/**
 * @brief Check for barrier synchronization at day start.
 * 
 * If a barrier is active for a new day, increment ready count and wait for release.
 *
 * @param[in] shm Shared memory pointer.
 * @param[in,out] last_synced_day Last synced day tracker.
 */
static void sync_check_day(sim_shm_t *shm, int *last_synced_day) {
    if (atomic_load(&shm->sync.barrier_active)) {
        unsigned int barrier_day = atomic_load(&shm->sync.day_seq);
        if ((int)barrier_day > *last_synced_day) {
            atomic_fetch_add(&shm->sync.ready_count, 1);
            *last_synced_day = (int)barrier_day;
            
            // Signal readiness
            pthread_mutex_lock(&shm->sync.mutex);
            pthread_cond_signal(&shm->sync.cond_workers_ready);
            
            // Wait for release, ensuring we are waiting for THIS day's barrier
            struct timespec ts;
            while (g_running && atomic_load(&shm->sync.barrier_active) &&
                   atomic_load(&shm->sync.day_seq) == barrier_day) {
                clock_gettime(CLOCK_MONOTONIC, &ts);
                ts.tv_sec += 1;
                pthread_cond_timedwait(&shm->sync.cond_day_start, &shm->sync.mutex, &ts);
            }
            pthread_mutex_unlock(&shm->sync.mutex);
        }
    }
}

typedef struct {
    int client_fd;
    sim_shm_t *shm;
} client_ctx_t;

/**
 * @brief Handle a single client connection task in thread pool.
 * @param[in] arg Pointer to client_ctx_t (takes ownership).
 */
static void handle_client_task(void *arg) {
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int client_fd = ctx->client_fd;
    sim_shm_t *shm = ctx->shm;
    free(ctx);

    // Receive protocolmessage with header
    po_header_t hdr;
    zcp_buffer_t *payload = NULL;
    int ret = net_recv_message(client_fd, &hdr, &payload);

    if (ret == 0 && payload) {
        // Validate message type and size
        if (hdr.msg_type != MSG_TYPE_TICKET_REQ || hdr.payload_len != sizeof(msg_ticket_req_t)) {
            LOG_ERROR("Invalid message: type=%u, len=%u (expected type=%u, len=%zu)",
                hdr.msg_type, hdr.payload_len, MSG_TYPE_TICKET_REQ, sizeof(msg_ticket_req_t));
            net_zcp_release_rx(payload);
            po_socket_close(client_fd);
            return;
        }

        // Extract request
        msg_ticket_req_t req;
        memcpy(&req, payload, sizeof(req));
        net_zcp_release_rx(payload);

        LOG_DEBUG("Request received from PID %d (TID %d), assigning ticket...", req.requester_pid,
            req.requester_tid);
        uint32_t ticket = atomic_fetch_add(&shm->ticket_seq, 1);
        LOG_DEBUG("Assigned ticket %u", ticket);
        if (ticket == UINT32_MAX)
            LOG_WARN("Ticket sequence wrapped around");

        int day, hour, min;
        get_sim_time(shm, &day, &hour, &min);
        LOG_INFO("[Day %d %02d:%02d] Issued ticket %u to PID %d for service %d", day, hour, min,
            ticket, req.requester_pid, req.service_type);

        // Send response with protocol header
        msg_ticket_resp_t resp = {.ticket_number = ticket, .assigned_service = req.service_type};
        ret = net_send_message(client_fd, MSG_TYPE_TICKET_RESP, PO_FLAG_NONE, 
            (const uint8_t *)&resp, (uint32_t)sizeof(resp));
        if (ret != 0) {
            LOG_ERROR("Failed to send ticket response (ret=%d, errno=%d: %s)", ret, errno, strerror(errno));
        } else {
            atomic_fetch_add(&shm->stats.total_tickets_issued, 1);
        }
    } else {
        LOG_DEBUG("Failed to receive message from client (ret=%d)", ret);
    }
    po_socket_close(client_fd);
}

/**
 * @brief Ticket Issuer entry point.
 * @param[in] argc Argument count.
 * @param[in] argv Argument vector.
 * @return 0 on success, >0 on failure.
 */
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // 0. Parse Args for Log Level and FD
    int server_fd = -1;
    int log_level = LOG_INFO;
    size_t pool_size = 64; // Default

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--socket-fd") == 0 && i + 1 < argc) {
            char *endptr;
            long val = strtol(argv[i + 1], &endptr, 10);
            if (*endptr == '\0' && val >= 0) {
                server_fd = (int)val;
            }
            i++;
        } else if ((strcmp(argv[i], "--loglevel") == 0 || strcmp(argv[i], "-l") == 0) &&
                   i + 1 < argc) {
            int l = po_logger_level_from_str(argv[i + 1]);
            if (l != -1)
                log_level = l;
            i++;
        } else if (strcmp(argv[i], "--pool-size") == 0 && i + 1 < argc) {
            char *endptr;
            unsigned long val = strtoul(argv[i + 1], &endptr, 10);
            if (*endptr == '\0' && val > 0) {
                pool_size = val;
            }
            i++;
        }
    }

    // ... (logger init) ...
    // 1. Initialize Logger
    po_logger_config_t log_cfg = {.level = log_level,
                                  .ring_capacity = 1024,
                                  .consumers = 1,
                                  .policy = LOGGER_OVERWRITE_OLDEST,
                                  .cacheline_bytes = PO_CACHE_LINE_MAX};
    if (po_logger_init(&log_cfg) != 0) {
        return 1;
    }
    po_logger_add_sink_file("logs/ticket_issuer.log", false);

    // 2. Attach SHM
    sim_shm_t *shm = sim_ipc_shm_attach();
    if (!shm) {
        po_logger_shutdown();
        return 1;
    }

    if (server_fd < 0) {
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return 1;
    }

    LOG_INFO("Ticket Issuer started on FD %d, Pool Size: %zu", server_fd, pool_size);

    // Initialize zero-copy pools for protocol messaging
    if (net_init_zerocopy(128, 128, 4096) != 0) {
        LOG_FATAL("Failed to initialize zero-copy pools");
        close(server_fd);
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return 1;
    }

    setup_signals();

    // Initialize Thread Pool
    threadpool_t *pool = tp_create(pool_size, 4096);
    if (!pool) {
        LOG_FATAL("Failed to create thread pool");
        close(server_fd);
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return 1;
    }

    // 4. Setup Poller
    poller_t *poller = poller_create();
    if (!poller) {
        LOG_FATAL("Failed to create poller");
        tp_destroy(pool, false);
        close(server_fd);
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return 1;
    }

    if (poller_add(poller, server_fd, EPOLLIN) != 0) {
        LOG_FATAL("Failed to add server_fd to poller");
        poller_destroy(poller);
        tp_destroy(pool, false);
        close(server_fd);
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return 1;
    }

    LOG_INFO("Poller initialized. Waiting for connections...");

    int last_synced_day = 0;

    // 5. Main Loop
    struct epoll_event events[32];
    while (g_running) {
        sync_check_day(shm, &last_synced_day);

        int n_events = poller_wait(poller, events, 32, 100);
        if (n_events > 0) {
            LOG_DEBUG("Poller woke up with %d events", n_events);
        }
        if (n_events < 0) {
            if (errno == EINTR)
                continue;
            LOG_ERROR("poller_wait failed: %s", po_strerror(errno));
            break;
        }

        for (int i = 0; i < n_events; i++) {
            if (events[i].data.fd == server_fd) {
                while (g_running) {
                    LOG_DEBUG("Accepting connection on FD %d...", server_fd);
                    int client_fd = po_socket_accept(server_fd, NULL, 0);
                    if (client_fd < 0) {
                        if (client_fd == PO_SOCKET_WOULDBLOCK) {
                            break;
                        }
                        if (errno == EINTR)
                            continue;
                        LOG_ERROR("accept failed: %s", po_strerror(errno));
                        break;
                    }

                    // Submit to Thread Pool
                    client_ctx_t *ctx = malloc(sizeof(client_ctx_t));
                    if (ctx) {
                        ctx->client_fd = client_fd;
                        ctx->shm = shm;
                        if (tp_submit(pool, handle_client_task, ctx) != 0) {
                            LOG_ERROR("Failed to submit task to pool");
                            free(ctx);
                            po_socket_close(client_fd);
                        }
                    } else {
                        LOG_ERROR("OOM allocating client context");
                        po_socket_close(client_fd);
                    }
                }
            }
        }
    }

    poller_destroy(poller);
    tp_destroy(pool, true);

    LOG_INFO("Ticket Issuer shutting down...");
    close(server_fd);
    sim_ipc_shm_detach(shm);
    net_shutdown_zerocopy();
    po_logger_shutdown();
    po_perf_shutdown(NULL);
    return 0;
}
