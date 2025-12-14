/**
 * @file ticket_issuer.c
 * @brief Ticket Issuer Process Implementation.
 */
#define _POSIX_C_SOURCE 200809L

#include <postoffice/net/socket.h>
#include <postoffice/net/net.h>
#include <postoffice/log/logger.h>
#include <utils/errors.h>
#include "../ipc/simulation_ipc.h"
#include "../ipc/simulation_protocol.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

int main(int argc, char** argv) {
    int user_id = 0;
    (void)argc; (void)argv;

    // 1. Initialize Logger
    po_logger_config_t log_cfg = {
        .level = LOG_INFO,
        .ring_capacity = 1024,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = 64
    };
    if (po_logger_init(&log_cfg) != 0) {
        return 1;
    }
    po_logger_add_sink_file("logs/ticket_issuer.log", false);

    // 2. Attach SHM
    sim_shm_t* shm = sim_ipc_shm_attach();
    if (!shm) {
        return 1;
    }

    // 3. Socket Handling (Inherited or Bind)
    int server_fd = -1;
    // Simple arg parsing for socket FD
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "--socket-fd") == 0 && i+1 < argc) {
            server_fd = atoi(argv[i+1]);
        }
    }

    if (server_fd < 0) {
        // Fallback or Error? Spec says "created by director", so error.
        // But for running standalone vs director, we might support bind.
        // For now, assume Refactor Goal: Director must create it.
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
            // LOG_ERROR("accept failed: %s", po_strerror(errno));
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

            // Log with Time
            sim_time_t *t = &shm->time_control;
            LOG_INFO("[Day %d %02d:%02d] Issued ticket %u to PID %d for service %d", 
                      atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute),
                      ticket, req.requester_pid, req.service_type);

            // Respond
            msg_ticket_resp_t resp = {
                .ticket_number = ticket,
                .assigned_service = req.service_type
            };
            po_socket_send(client_fd, &resp, sizeof(resp), 0);
            
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
