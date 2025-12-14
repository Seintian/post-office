/**
 * @file ticket_issuer.c
 * @brief Ticket Issuer Process Implementation.
 */
#define _POSIX_C_SOURCE 200809L

#include <postoffice/net/socket.h>
#include <postoffice/net/net.h>
#include <postoffice/log/logger.h>
#include <utils/errors.h>
#include <postoffice/perf/cache.h>
#include "../ipc/simulation_ipc.h"
#include "../ipc/simulation_protocol.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/socket.h>
#include <utils/signals.h>

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig, siginfo_t* info, void* context) {
    (void)sig; (void)info; (void)context;
    g_running = 0;
}

static void setup_signals(void) {
    if (sigutil_handle_terminating(handle_signal, 0) != 0) {
        LOG_WARN("Failed to register signal handlers");
    }
}

static void get_sim_time(sim_shm_t* shm, int *d, int *h, int *m) {
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *d = (packed >> 16) & 0xFFFF;
    *h = (packed >> 8) & 0xFF;
    *m = packed & 0xFF;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    // 1. Initialize Logger
    po_logger_config_t log_cfg = {
        .level = LOG_INFO,
        .ring_capacity = 1024,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = PO_CACHE_LINE_MAX
    };
    if (po_logger_init(&log_cfg) != 0) {
        return 1;
    }
    po_logger_add_sink_file("logs/ticket_issuer.log", false);

    // 2. Attach SHM
    sim_shm_t* shm = sim_ipc_shm_attach();
    if (!shm) {
        po_logger_shutdown();
        return 1;
    }

    // 3. Socket Handling
    int server_fd = -1;
    // Simple arg parsing for socket FD
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "--socket-fd") == 0 && i+1 < argc) {
            char* endptr;
            errno = 0;
            long val = strtol(argv[i+1], &endptr, 10);
            if (errno == 0 && *endptr == '\0' && val >= 0 && val <= INT_MAX) {
                server_fd = (int)val;
            }
        }
    }

    if (server_fd < 0) {
        // Fallback or Error? Spec says "created by director", so error.
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return 1;
    }

    LOG_INFO("Ticket Issuer started on FD %d", server_fd);

    setup_signals();

    // 4. Main Loop
    while (g_running) {
        // Accept connection
        int client_fd = po_socket_accept(server_fd, NULL, 0);
        if (client_fd < 0) {
            if (client_fd == PO_SOCKET_WOULDBLOCK) {
                usleep(1000); 
                continue;
            }
            if (errno == EINTR) continue;
            LOG_ERROR("accept failed: %s", po_strerror(errno));
            continue;
        }

        // Handle Client
        msg_ticket_req_t req;
        ssize_t n = -1;
        int attempts = 0;
        while (attempts++ < 100) {
            n = po_socket_recv(client_fd, &req, sizeof(req), 0);
            if (n == sizeof(req)) break;
            if (n == PO_SOCKET_WOULDBLOCK) {
                usleep(5000);
                continue;
            }
            // 0 (EOF) or -1 (Error)
            break;
        }

        if (n == sizeof(req)) {
            // Logic: Increment Global Ticket Seq
            uint32_t ticket = atomic_fetch_add(&shm->ticket_seq, 1);
             if (ticket == UINT32_MAX) {
                LOG_WARN("Ticket sequence wrapped around");
            }

            // Log with Time
            int day, hour, min;
            get_sim_time(shm, &day, &hour, &min);
            LOG_INFO("[Day %d %02d:%02d] Issued ticket %u to PID %d for service %d", 
                      day, hour, min,
                      ticket, req.requester_pid, req.service_type);

            // Respond
            msg_ticket_resp_t resp = {
                .ticket_number = ticket,
                .assigned_service = req.service_type
            };
            ssize_t sent = po_socket_send(client_fd, &resp, sizeof(resp), 0);
            if (sent != sizeof(resp)) {
                LOG_ERROR("Failed to send ticket response to PID %d: %s", 
                           req.requester_pid, po_strerror(errno));
            }

            // Update Stats
            atomic_fetch_add(&shm->stats.total_tickets_issued, 1);
        }

        po_socket_close(client_fd);
    }

    LOG_INFO("Ticket Issuer shutting down...");
    // server_fd is closed by OS or Director? Usually child closes its copy.
    close(server_fd); 
    sim_ipc_shm_detach(shm);
    po_logger_shutdown();
    return 0;
}
