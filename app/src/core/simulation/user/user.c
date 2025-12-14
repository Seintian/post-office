/**
 * @file user.c
 * @brief User Process Implementation.
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
#include <unistd.h>
#include <sys/sem.h>
#include <time.h>
#include <utils/signals.h>

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig, siginfo_t* info, void* context) {
    (void)sig; (void)info; (void)context;
    g_running = 0;
}

static void setup_signals(void) {
    if (sigutil_handle_terminating(handle_signal, 0) != 0) {
        // Log not initialized yet likely, but try
         fprintf(stderr, "Failed to register signal handlers\n");
    }
}

int main(int argc, char** argv) {
    // Expected args: ./user <user_id> <service_type>
    int user_id = 0;    // Simulation ID (not PID)
    int service_type = 0;

    int opt;
    while ((opt = getopt(argc, argv, "i:s:")) != -1) {
        switch (opt) {
            case 'i': user_id = atoi(optarg); break;
            case 's': service_type = atoi(optarg); break;
            default: break;
        }
    }

    // 1. Init Logger (minimal buffer for user, or share setting)
    po_logger_config_t log_cfg = {
        .level = LOG_INFO,
        .ring_capacity = 256,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = 64
    };
    if (po_logger_init(&log_cfg) != 0) return 1;

    // Log to shared users log file (append mode)
    po_logger_add_sink_file("logs/users.log", true);
    // Optional: Log to console if needed for debugging
    // po_logger_add_sink_console(true);

    // 2. Attach SHM
    sim_shm_t* shm = sim_ipc_shm_attach();
    if (!shm) {
        return 1;
    }

    setup_signals();

    sim_time_t *t = &shm->time_control;

    // 3. Connect to Ticket Issuer
    int sock_fd = -1;
    for (int i = 0; i < 100; i++) {
        sock_fd = po_socket_connect_unix(SIM_SOCK_ISSUER);
        if (sock_fd >= 0) break;
        usleep(20000); // 20ms retry
    }

    if (sock_fd < 0) {
        LOG_ERROR("[Day %d %02d:%02d] User process failed to connect to socket", 
                  atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute));
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return 1;
    }

    // 4. Request Ticket
    msg_ticket_req_t req = {
        .requester_pid = getpid(),
        .service_type = (service_type_t)service_type
    };
    po_socket_send(sock_fd, &req, sizeof(req), 0);

    msg_ticket_resp_t resp;
    ssize_t n = -1;
    // Simple blocking loop for short message
    int attempts = 0;
    while (attempts++ < 100) {
        n = po_socket_recv(sock_fd, &resp, sizeof(resp), 0);
        if (n == sizeof(resp)) break;
        if (n == PO_SOCKET_WOULDBLOCK) {
            usleep(5000);
            continue;
        }
        // Hard error or partial read (not handled for simple struct here)
        break;
    }

    po_socket_close(sock_fd);

    if (n != sizeof(resp)) {
        LOG_ERROR("User %d failed to receive ticket (n=%zd, errno=%d)", user_id, n, errno);
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return 1;
    }

    LOG_INFO("[Day %d %02d:%02d] User %d (PID %d) got ticket %u for service %d", 
             atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute),
             user_id, getpid(), resp.ticket_number, resp.assigned_service);

    // 5. Enter Queue (Increment "Waiting" Semaphore)
    // Actually, Semaphore semantics: 
    // If resource (worker) is available, sem value > 0.
    // If we want to signal "I am waiting", we might Increment a "Users Waiting" semaphore that Worker waits on?
    // In `worker.c`, I implemented `semop(..., -1)`. This means Worker "Takes" a unit.
    // So User must "Give" (Post/Signal) a unit.
    // Yes.

    int semid = sim_ipc_sem_get();
    if (semid != -1) {
        struct sembuf sb = {
            .sem_num = (unsigned short)service_type,
            .sem_op = 1, // Increment (Post user availability)
            .sem_flg = 0
        };
        semop(semid, &sb, 1);

        // Also update SHM stats for visibility
        atomic_fetch_add(&shm->queues[service_type].waiting_count, 1);
    }

    // 6. Wait for Service
    // Loop checking if any worker is serving my ticket
    bool served = false;
    while (g_running && !served) {
        // Check if simulation is still active
        if (!atomic_load(&shm->time_control.sim_active)) break;

        // Check if I am being served
        // This linear scan is inefficient for many workers but fine for <100
        for (int i = 0; i < SIM_MAX_WORKERS; i++) {
            // Need atomic load here
            uint32_t current_t = atomic_load(&shm->workers[i].current_ticket);
            // int current_s = atomic_load(&shm->workers[i].service_type);

            // Check if this worker handles my service and is serving my ticket
            // But wait, ticket numbers are global unique.
            if (current_t == resp.ticket_number) {
                LOG_INFO("[Day %d %02d:%02d] User %d being served by Worker %d", 
                         atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute),
                         user_id, i);
                served = true;
                break;
            }
        }

        if (!served) {
            usleep(50000); // 50ms poll
        }
    }

    // 7. Being Served (maybe wait until worker is done?)
    // If we detected we are being served, we should wait until worker finishes?
    // Worker sets current_ticket = ticket, then sleeps, then sets current_ticket = 0/next.
    // So if we see it, we are being served.
    // We can loop until we see it change or we are done.
    if (served) {
        while (g_running) {
            // Find my worker again (or remember index)
            bool still_serving = false;
            for (int i = 0; i < SIM_MAX_WORKERS; i++) {
                if (atomic_load(&shm->workers[i].current_ticket) == resp.ticket_number) {
                    still_serving = true;
                    break;
                }
            }
            if (!still_serving) {
                LOG_INFO("[Day %d %02d:%02d] User %d service completed", 
                         atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute), user_id);
                break; 
            }
            usleep(50000);
        }
    } else {
        LOG_WARN("[Day %d %02d:%02d] User %d gave up or sim ended", 
                 atomic_load(&t->day), atomic_load(&t->hour), atomic_load(&t->minute), user_id);
    }

    sim_ipc_shm_detach(shm);
    po_logger_shutdown();
    return 0;
}
