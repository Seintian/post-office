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
#include <sys/syscall.h>

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

// Standalone initialization
int user_standalone_init(sim_shm_t** out_shm) {
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

int user_run(int user_id, int service_type, sim_shm_t* shm, volatile _Atomic bool *active_flag) {
    if (!shm) {
        LOG_FATAL("user_run called with NULL shm");
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

    LOG_INFO("User %d starting with %d requests (TID: %ld)", user_id, n_requests, (long)gettid());

    for (int req = 0; req < n_requests && g_running; req++) {
        if (active_flag && !atomic_load(active_flag)) break;

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
            if (active_flag && !atomic_load(active_flag)) break;
            sock_fd = po_socket_connect_unix(sock_path);
            if (sock_fd >= 0) break;
            LOG_DEBUG("Connect attempt %d failed, retrying...", i+1);
            usleep(20000); // 20ms retry
        }

        if (active_flag && !atomic_load(active_flag)) {
            if (sock_fd >= 0) po_socket_close(sock_fd);
            break;
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

        // Set Receive Timeout to allow flag checking
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms
        setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        // Request Ticket
        msg_ticket_req_t msg_req = {
            .requester_pid = getpid(),
            .requester_tid = (pid_t)syscall(SYS_gettid),
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
            if (active_flag && !atomic_load(active_flag)) break;

            n = po_socket_recv(sock_fd, &resp, sizeof(resp), 0);
            if (n == sizeof(resp)) break;
            if (n == PO_SOCKET_WOULDBLOCK || (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
                // Timeout or non-blocking
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

        // Working Hours Check: 08:00 to 17:00
        // Users should not enter queue if office is closed
        if (hour < 8 || hour >= 17) {
            LOG_DEBUG("[Day %d %02d:%02d] User %d found office closed. Waiting for opening...", day, hour, min, user_id);

            // Calculate minutes to wait until 08:00
            int current_total_mins = hour * 60 + min;
            int target_total_mins = 8 * 60; // 08:00
            int mins_to_wait = 0;

            if (hour >= 17) {
                // Wait until midnight + 8 hours
                int mins_until_midnight = (24 * 60) - current_total_mins;
                mins_to_wait = mins_until_midnight + target_total_mins;
            } else { // hour < 8
                mins_to_wait = target_total_mins - current_total_mins;
            }
            
            if (mins_to_wait > 0) {
                // params.tick_nanos = ns per sim minute
                uint64_t total_nanos = (uint64_t)mins_to_wait * shm->params.tick_nanos;
                struct timespec ts;
                ts.tv_sec = (time_t)(total_nanos / 1000000000ULL);
                ts.tv_nsec = (long)(total_nanos % 1000000000ULL);

                // Wait exactly the simulation time needed
                nanosleep(&ts, NULL);
            } else {
                usleep(10000); // Fallback
            }
            continue;
        }

        LOG_DEBUG("[Day %d %02d:%02d] User %d (PID %d) got ticket %u for service %d (Req %d/%d)", 
                    day, hour, min,
                    user_id, getpid(), resp.ticket_number, resp.assigned_service, req+1, n_requests);

        // Enter Queue
        int semid = sim_ipc_sem_get();
        if (semid != -1) {
            atomic_fetch_add(&shm->queues[service_type].waiting_count, 1);

            // Push Ticket to Shared Queue
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
            LOG_DEBUG("User %d waiting for queue semaphore (type %d)...", user_id, service_type);
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
        
        LOG_DEBUG("User %d entered queue for service %d", user_id, service_type);

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
                // LOG_DEBUG("User %d still waiting for service...", user_id); // Too spammy? Maybe un-comment if deep debug needed
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
    // No detach here, managed by caller or thread exit (OS)
    return 0;
}
