#define _POSIX_C_SOURCE 200809L

#include "user_loop.h"
#include <postoffice/net/socket.h>
#include <postoffice/net/net.h>
#include <postoffice/log/logger.h>
#include <utils/errors.h>
#include <postoffice/random/random.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>
#include <postoffice/sysinfo/sysinfo.h>
#include "../../ipc/simulation_ipc.h"
#include "../../ipc/simulation_protocol.h"
#include "utils/configs.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>
#include <time.h>
#include <utils/signals.h>
#include <stdatomic.h>

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig, siginfo_t* info, void* context) {
    (void)sig; (void)info; (void)context;
    g_running = 0;
}

static void setup_signals(void) {
    if (sigutil_handle_terminating(handle_signal, 0) != 0) {
        fprintf(stderr, "Failed to register signal handlers\n");
    }
}

static void get_sim_time(sim_shm_t* shm, int *d, int *h, int *m) {
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *d = (packed >> 16) & 0xFFFF;
    *h = (packed >> 8) & 0xFF;
    *m = packed & 0xFF;
}

static int user_init(int user_id, sim_shm_t** out_shm) {
    (void)user_id;
    // Collect system information for optimizations
    po_sysinfo_t sysinfo;
    
    size_t cacheline_size = 64; // default
    if (po_sysinfo_collect(&sysinfo) == 0 && sysinfo.dcache_lnsize > 0) {
        cacheline_size = (size_t)sysinfo.dcache_lnsize;
    }

    if (po_metrics_init() != 0) {
        fprintf(stderr, "user: metrics init failed\n");
    }

    int level = LOG_INFO;
    char *env = getenv("PO_LOG_LEVEL");
    if (env) {
         int l = po_logger_level_from_str(env);
         if (l != -1) level = l;
    }

    po_logger_config_t log_cfg = {
        .level = level,
        .ring_capacity = 256,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = cacheline_size
    };
    if (po_logger_init(&log_cfg) != 0) {
        fprintf(stderr, "user: logger init failed\n");
        return -1;
    }
    po_logger_add_sink_file("logs/users.log", true);

    sim_shm_t* shm = sim_ipc_shm_attach();
    if (!shm) {
        return -1;
    }
    *out_shm = shm;

    setup_signals();
    po_rand_seed_auto(); 

    return 0;
}

int user_run(int user_id, int service_type) {
    sim_shm_t* shm = NULL;

    if (user_init(user_id, &shm) != 0) {
        if (shm) sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return EXIT_FAILURE;
    }

    int day, hour, min;
    get_sim_time(shm, &day, &hour, &min);

    // Read N_REQUESTS
    int n_requests = 1; // default
    char *env_req = getenv("PO_USER_REQUESTS");
    if (env_req) {
        long val = strtol(env_req, NULL, 10);
        if (val > 0) n_requests = (int)val;
    }

    LOG_INFO("User %d starting with %d requests", user_id, n_requests);

    for (int req = 0; req < n_requests && g_running; req++) {
        // Optional: Random delay between requests
        if (req > 0) {
            usleep((unsigned int)po_rand_range_i64(100000, 500000)); // 100-500ms
        }

        // Connect to Ticket Issuer
        const char* user_home = getenv("HOME");
        char sock_path[512];
        if (user_home) {
            snprintf(sock_path, sizeof(sock_path), "%s/.postoffice/issuer.sock", user_home);
        } else {
            snprintf(sock_path, sizeof(sock_path), "/tmp/postoffice_%d_issuer.sock", getuid());
        }

        int sock_fd = -1;
        for (int i = 0; i < 100 && g_running; i++) {
            sock_fd = po_socket_connect_unix(sock_path);
            if (sock_fd >= 0) break;
            usleep(20000); // 20ms retry
        }

        if (sock_fd < 0 && g_running) {
            get_sim_time(shm, &day, &hour, &min);
            LOG_ERROR("[Day %d %02d:%02d] User process failed to connect to socket %s", 
                      day, hour, min, sock_path);
            break; 
        }

        if (!g_running) {
            if (sock_fd >= 0) po_socket_close(sock_fd);
            break;
        }

        // Request Ticket
        msg_ticket_req_t msg_req = {
            .requester_pid = getpid(),
            .service_type = (service_type_t)service_type
        };
        ssize_t sent = po_socket_send(sock_fd, &msg_req, sizeof(msg_req), 0);
        if (sent != sizeof(msg_req)) {
            LOG_ERROR("User %d failed to send ticket request (sent=%zd, errno=%d)", user_id, sent, errno);
            po_socket_close(sock_fd);
            break;
        }

        msg_ticket_resp_t resp;
        ssize_t n = -1;
        int attempts = 0;
        while (attempts++ < 100 && g_running) {
            n = po_socket_recv(sock_fd, &resp, sizeof(resp), 0);
            if (n == sizeof(resp)) break;
            if (n == PO_SOCKET_WOULDBLOCK) {
                usleep(5000);
                continue;
            }
            break;
        }

        po_socket_close(sock_fd);

        if (n != sizeof(resp)) {
            if (g_running) {
                 LOG_ERROR("User %d failed to receive ticket (n=%zd, errno=%d)", user_id, n, errno);
            }
            break;
        }

        get_sim_time(shm, &day, &hour, &min);
        LOG_DEBUG("[Day %d %02d:%02d] User %d (PID %d) got ticket %u for service %d (Req %d/%d)", 
                 day, hour, min,
                 user_id, getpid(), resp.ticket_number, resp.assigned_service, req+1, n_requests);

        // Enter Queue
    int semid = sim_ipc_sem_get();
    if (semid != -1) {
        atomic_fetch_add(&shm->queues[service_type].waiting_count, 1);

        // Push Ticket to Shared Queue
        // Note: EXPLODE_THRESHOLD=100 ensures we rarely wrap/overflow 128 buffer 
        // if users respect it. 
        queue_status_t *q = &shm->queues[service_type];
        unsigned int tail = atomic_fetch_add(&q->tail, 1);
        unsigned int idx = tail % 128;
        // Wait for slot (should be 0)
        while (atomic_load(&q->tickets[idx]) != 0) {
            // Busy wait / yield. 
            // In a robust system we handles wrap better or larger buffer.
             usleep(100);
        }
        atomic_store(&q->tickets[idx], resp.ticket_number + 1);

        struct sembuf sb = {
            .sem_num = (unsigned short)service_type,
            .sem_op = 1, // Increment
            .sem_flg = 0
        };
        while (semop(semid, &sb, 1) == -1) {
            if (errno == EINTR) {
                if (!g_running) {
                    atomic_fetch_sub(&shm->queues[service_type].waiting_count, 1);
                    goto shutdown;
                }
                continue;
            }
            LOG_WARN("User %d semop failed (errno=%d)", user_id, errno);
            atomic_fetch_sub(&shm->queues[service_type].waiting_count, 1);
            goto shutdown; 
        }
    } else {
            LOG_WARN("User %d failed to get semid", user_id);
            break;
        }

        // Wait for Service
        bool served = false;
        while (g_running && !served) {
            if (!atomic_load(&shm->time_control.sim_active)) break;

            for (uint32_t i = 0; i < shm->params.n_workers; i++) {
                uint32_t current_t = atomic_load(&shm->workers[i].current_ticket);
                if (current_t == resp.ticket_number) {
                    get_sim_time(shm, &day, &hour, &min);
                    LOG_DEBUG("[Day %d %02d:%02d] User %d being served by Worker %d", 
                             day, hour, min,
                             user_id, i);
                    served = true;
                    break;
                }
            }

            if (!served) {
                usleep(50000);
            }
        }

        if (served) {
            while (g_running) {
                bool still_serving = false;
                for (uint32_t i = 0; i < shm->params.n_workers; i++) {
                    if (atomic_load(&shm->workers[i].current_ticket) == resp.ticket_number) {
                        still_serving = true;
                        break;
                    }
                }
                if (!still_serving) {
                    get_sim_time(shm, &day, &hour, &min);
                    LOG_DEBUG("[Day %d %02d:%02d] User %d service completed", 
                             day, hour, min, user_id);
                    break; 
                }
                usleep(50000);
            }
        } else {
            get_sim_time(shm, &day, &hour, &min);
            LOG_WARN("[Day %d %02d:%02d] User %d gave up or sim ended", 
                     day, hour, min, user_id);
            break;
        }
    }

shutdown:
    sim_ipc_shm_detach(shm);
    po_logger_shutdown();
    po_metrics_shutdown();
    po_perf_shutdown(stdout);
    return 0;
}
