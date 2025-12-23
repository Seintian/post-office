#define _POSIX_C_SOURCE 200809L

#include "user_loop.h"

#include <errno.h>
#include <postoffice/log/logger.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/net/net.h>
#include <postoffice/net/socket.h>
#include <postoffice/perf/perf.h>
#include <postoffice/random/random.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/syscall.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <utils/errors.h>
#include <utils/signals.h>

#include "../../ipc/simulation_ipc.h"
#include "../../ipc/simulation_protocol.h"
#include "utils/configs.h"

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
 * @brief Register terminating signal handlers.
 */
static void setup_signals(void) {
    if (sigutil_handle_terminating(handle_signal, 0) != 0) {
        fprintf(stderr, "Failed to register signal handlers\n");
    }
}

/**
 * @brief Extract sim time from SHM.
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
 * @brief Initialize user standalone resources (SHM, Logger).
 * @param[out] out_shm Pointer to receive SHM address.
 * @return 0 on success, -1 on failure.
 */
int user_standalone_init(sim_shm_t **out_shm) {
    // Collect system information for optimizations
    po_sysinfo_t sysinfo;
    size_t cacheline_size = 64; // default
    if (po_sysinfo_collect(&sysinfo) == 0 && sysinfo.dcache_lnsize > 0) {
        cacheline_size = (size_t)sysinfo.dcache_lnsize;
    }

    if (po_metrics_init(0, 0, 0) != 0) {
        fprintf(stderr, "user: metrics init failed\n");
    }

    int level = LOG_INFO;
    char *env = getenv("PO_LOG_LEVEL");
    if (env) {
        int l = po_logger_level_from_str(env);
        if (l != -1)
            level = l;
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

    sim_shm_t *shm = sim_ipc_shm_attach();
    if (!shm) {
        return -1;
    }
    *out_shm = shm;

    // Initialize zero-copy pools for protocol messaging
    if (net_init_zerocopy(32, 32, 4096) != 0) {
        fprintf(stderr, "user: Failed to initialize zero-copy pools\n");
        sim_ipc_shm_detach(shm);
        po_logger_shutdown();
        return -1;
    }

    setup_signals();
    po_rand_seed_auto();

    return 0;
}

/**
 * @brief Cleanup user resources.
 * @param[in] shm SHM pointer to detach.
 */
void user_standalone_cleanup(sim_shm_t *shm) {
    net_shutdown_zerocopy();
    if (shm) {
        sim_ipc_shm_detach(shm);
    }
    po_logger_shutdown();
}

/**
 * @brief Main user lifecycle loop.
 * 
 * Requests ticket, enters queue, waits for service completion.
 *
 * @param[in] user_id Unique User ID.
 * @param[in] service_type Service type to request.
 * @param[in] shm Shared memory pointer.
 * @param[in] active_flag Optional cancellation flag (for thread pool usage).
 * @return 0 on success.
 */
int user_run(int user_id, int service_type, sim_shm_t *shm, volatile _Atomic bool *active_flag) {
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
        if (val > 0)
            n_requests = (int)val;
    }

    LOG_INFO("User %d starting with %d requests (TID: %ld)", user_id, n_requests, (long)gettid());

    for (int req = 0; req < n_requests && g_running; req++) {
        if (active_flag && !atomic_load(active_flag))
            break;

        // Optional: Random delay between requests
        if (req > 0) {
            usleep((unsigned int)po_rand_range_i64(100000, 500000)); // 100-500ms
        }

        // Connect to Ticket Issuer
        const char *user_home = getenv("HOME");
        char sock_path[512];
        if (user_home) {
            snprintf(sock_path, sizeof(sock_path), "%s/.postoffice/issuer.sock", user_home);
        } else {
            snprintf(sock_path, sizeof(sock_path), "/tmp/postoffice_%d_issuer.sock", getuid());
        }

        int sock_fd = -1;
        for (int i = 0; i < 100 && g_running; i++) {
            if (active_flag && !atomic_load(active_flag))
                break;
            sock_fd = po_socket_connect_unix(sock_path);
            if (sock_fd >= 0)
                break;
            LOG_DEBUG("Connect attempt %d failed, retrying...", i + 1);
            usleep(20000); // 20ms retry
        }

        if (active_flag && !atomic_load(active_flag)) {
            if (sock_fd >= 0)
                po_socket_close(sock_fd);
            break;
        }

        if (sock_fd < 0 && g_running) {
            get_sim_time(shm, &day, &hour, &min);
            LOG_ERROR("[Day %d %02d:%02d] User process failed to connect to socket %s", day, hour,
                      min, sock_path);
            break;
        }

        if (!g_running) {
            if (sock_fd >= 0)
                po_socket_close(sock_fd);
            break;
        }

        // Set Receive Timeout to allow time for server thread pool processing
        struct timeval tv;
        tv.tv_sec = 5; // 5 second timeout (thread pool may have queued tasks)
        tv.tv_usec = 0;
        setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

        // Request Ticket with protocol header
        msg_ticket_req_t msg_req = {
            .requester_pid = getpid(),
            .requester_tid = (pid_t)syscall(SYS_gettid),
            .service_type = (service_type_t)service_type
        };
        int ret = net_send_message(sock_fd, MSG_TYPE_TICKET_REQ, PO_FLAG_NONE,
            (const uint8_t *)&msg_req, (uint32_t)sizeof(msg_req));
        if (ret != 0) {
            LOG_ERROR("User %d failed to send ticket request (ret=%d)", user_id, ret);
            po_socket_close(sock_fd);
            break;
        }

        // Receive response with protocol header
        po_header_t hdr;
        zcp_buffer_t *payload = NULL;
        ret = net_recv_message(sock_fd, &hdr, &payload);

        po_socket_close(sock_fd);

        if (ret != 0 || !payload) {
            if (g_running) {
                LOG_ERROR("User %d failed to receive ticket (ret=%d)", user_id, ret);
            }
            if (payload)
                net_zcp_release_rx(payload);
            break;
        }

        // Validate response
        if (hdr.msg_type != MSG_TYPE_TICKET_RESP || hdr.payload_len != sizeof(msg_ticket_resp_t)) {
            LOG_ERROR("User %d received invalid response: type=%u, len=%u", 
                     user_id, hdr.msg_type, hdr.payload_len);
            net_zcp_release_rx(payload);
            break;
        }

        msg_ticket_resp_t resp;
        memcpy(&resp, payload, sizeof(resp));
        net_zcp_release_rx(payload);

        get_sim_time(shm, &day, &hour, &min);

        // Working Hours Check: 08:00 to 17:00
        // Users should not enter queue if office is closed
        if (hour < 8 || hour >= 17) {
            LOG_DEBUG("[Day %d %02d:%02d] User %d found office closed. Waiting for opening...", day,
                      hour, min, user_id);

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
                // Wait loop synchronized with Director's tick
                int target_day = day;
                if (hour >= 17) target_day++; 
                
                pthread_mutex_lock(&shm->time_control.mutex);
                while (g_running) {
                    if (active_flag && !atomic_load(active_flag))
                        break;
                        
                    get_sim_time(shm, &day, &hour, &min);
                    // Check if we reached 08:00 of the next/target day
                    // Simpler check: Just wait until office is open (8 <= hour < 17)
                    if (hour >= 8 && hour < 17) {
                        break;
                    }
                    
                    struct timespec ts;
                    clock_gettime(CLOCK_MONOTONIC, &ts);
                    ts.tv_sec += 1;
                    pthread_cond_timedwait(&shm->time_control.cond_tick, &shm->time_control.mutex, &ts);
                }
                pthread_mutex_unlock(&shm->time_control.mutex);
            } else {
                usleep(10000); // Fallback
            }
            continue;
        }

        LOG_DEBUG("[Day %d %02d:%02d] User %d (PID %d) got ticket %u for service %d (Req %d/%d)",
                  day, hour, min, user_id, getpid(), resp.ticket_number, resp.assigned_service,
                  req + 1, n_requests);

        // Enter Queue
        // Use Mutex/Cond instead of Semaphores
        queue_status_t *q = &shm->queues[service_type];
        
        atomic_fetch_add(&q->waiting_count, 1);

        // Push Ticket to Shared Queue
        unsigned int tail = atomic_fetch_add(&q->tail, 1);
        unsigned int idx = tail % 128;
        // Wait for slot (should be 0)
        while (atomic_load(&q->tickets[idx]) != 0) {
            // Busy wait / yield.
            usleep(100);
        }
        atomic_store(&q->tickets[idx], resp.ticket_number + 1);

        // Signal Worker
        pthread_mutex_lock(&q->mutex);
        LOG_TRACE("User %d signaling cond_added for service %d", user_id, service_type);
        pthread_cond_signal(&q->cond_added);
        pthread_mutex_unlock(&q->mutex);
        
        LOG_DEBUG("User %d entered queue for service %d", user_id, service_type);

        // Wait for Service
        bool served = false;
        
        pthread_mutex_lock(&q->mutex);
        while (g_running && !served) {
            if (active_flag && !atomic_load(active_flag))
                break;

            if (!atomic_load(&shm->time_control.sim_active))
                break;

            for (uint32_t i = 0; i < shm->params.n_workers; i++) {
                uint32_t current_t = atomic_load(&shm->workers[i].current_ticket);
                if (current_t == resp.ticket_number) {
                    get_sim_time(shm, &day, &hour, &min);
                    LOG_DEBUG("[Day %d %02d:%02d] User %d being served by Worker %d", day, hour,
                              min, user_id, i);
                    served = true;
                    break;
                }
            }

            if (!served) {
                // Wait for any worker to change state (pick up ticket)
                // Worker broadcasts cond_served when picking up ticket too.
                LOG_TRACE("User %d waiting on cond_served (waiting for pickup)", user_id);
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                ts.tv_sec += 1;
                pthread_cond_timedwait(&q->cond_served, &q->mutex, &ts);
                LOG_TRACE("User %d woke up from cond_served (waiting for pickup)", user_id);
            }
        }
        pthread_mutex_unlock(&q->mutex);

        // Double check cancellation before processing service result
        if (active_flag && !atomic_load(active_flag)) {
            served = false; // Treat as if we gave up
        }

        if (served) {
            pthread_mutex_lock(&q->mutex);
            while (g_running) {
                if (active_flag && !atomic_load(active_flag))
                    break;

                bool still_serving = false;
                for (uint32_t i = 0; i < shm->params.n_workers; i++) {
                    if (atomic_load(&shm->workers[i].current_ticket) == resp.ticket_number) {
                        still_serving = true;
                        break;
                    }
                }
                if (!still_serving) {
                    get_sim_time(shm, &day, &hour, &min);
                    LOG_DEBUG("[Day %d %02d:%02d] User %d service completed", day, hour, min,
                              user_id);
                    break;
                }
                // Wait for completion signal
                LOG_TRACE("User %d waiting on cond_served (waiting for completion)", user_id);
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                ts.tv_sec += 1;
                pthread_cond_timedwait(&q->cond_served, &q->mutex, &ts);
                LOG_TRACE("User %d woke up from cond_served (waiting for completion)", user_id);
            }
            pthread_mutex_unlock(&q->mutex);
        } else {
            get_sim_time(shm, &day, &hour, &min);
            LOG_WARN("[Day %d %02d:%02d] User %d gave up or sim ended", day, hour, min, user_id);
            break;
        }
    }

    // No detach here, managed by caller or thread exit (OS)
    return 0;
}
