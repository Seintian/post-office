/**
 * @file user_loop.c
 * @brief Implementation of the User Simulation Loop using shared logic.
 */

#define _POSIX_C_SOURCE 200809L

#include "user_loop.h"
#include "ipc/sim_client.h"
#include "ipc/simulation_ipc.h"
#include "ipc/simulation_protocol.h"
#include "utils/configs.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <errno.h>

#include <postoffice/log/logger.h>
#include <postoffice/net/net.h>
#include <postoffice/net/socket.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/random/random.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <utils/signals.h>

// --- Initialization ---

// Signal handler for User Process (Standalone) uses global, but threaded users use pointer flag.
// We'll keep local shutdown handler just in case it's run as process.
static volatile sig_atomic_t g_proc_shutdown = 0;
static void on_sig(int s, siginfo_t* i, void* c) { (void)s; (void)i; (void)c; g_proc_shutdown = 1; }

int initialize_user_runtime(sim_shm_t **out_shm) {
    po_sysinfo_t si;
    size_t cache = (po_sysinfo_collect(&si)==0 && si.dcache_lnsize>0) ? (size_t)si.dcache_lnsize : 64;

    po_metrics_init(0, 0, 0);

    char *lvl = getenv("PO_LOG_LEVEL");
    po_logger_init(&(po_logger_config_t){
        .level = (lvl && po_logger_level_from_str(lvl)!=-1) ? po_logger_level_from_str(lvl) : LOG_INFO,
        .ring_capacity = 256, .consumers=1, .cacheline_bytes=cache
    });
    po_logger_add_sink_file("logs/users.log", true);
    po_logger_add_sink_console(true);

    *out_shm = sim_ipc_shm_attach();
    if (!*out_shm) {
        LOG_FATAL("User Runtime: Failed to attach shared memory");
        return -1;
    }

    if (net_init_zerocopy(32, 32, 4096) != 0) {
        LOG_FATAL("User Runtime: Failed to initialize network zerocopy");
        return -1;
    }

    sim_client_setup_signals(on_sig);
    atomic_fetch_add(&((*out_shm)->stats.connected_users), 1);
    po_rand_seed_auto();
    return 0;
}

void teardown_user_runtime(sim_shm_t *shm) {
    if (shm) {
        atomic_fetch_sub(&shm->stats.connected_users, 1);
        sim_ipc_shm_detach(shm);
    }
    net_shutdown_zerocopy();
    po_logger_shutdown();
}

static bool obtain_ticket(sim_shm_t *shm, int service_type, 
                         volatile _Atomic bool *should_continue, uint32_t *ticket_out) {
    int fd = sim_client_connect_issuer(should_continue, shm);
    if (fd < 0) {
        LOG_WARN("User failed to connect to Ticket Issuer");
        return false;
    }

    // Timeout
    struct timeval tv = { .tv_usec = 500000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    msg_ticket_req_t req = {
        .requester_pid = getpid(),
        .requester_tid = (pid_t)syscall(SYS_gettid),
        .service_type = (service_type_t)service_type
    };

    if (net_send_message(fd, MSG_TYPE_TICKET_REQ, PO_FLAG_NONE, (uint8_t*)&req, sizeof(req)) != 0) {
        po_socket_close(fd);
        return false;
    }

    po_header_t h; 
    zcp_buffer_t *p = NULL;
    int ret = net_recv_message_blocking(fd, &h, &p); // Blocking until timeout
    po_socket_close(fd);

    if (ret != 0 || !p || h.msg_type != MSG_TYPE_TICKET_RESP) {
        if (p) net_zcp_release_rx(p);
        LOG_WARN("User failed to receive valid Ticket Response (ret=%d, type=0x%02X)", ret, h.msg_type);
        return false;
    }

    msg_ticket_resp_t resp;
    memcpy(&resp, p, sizeof(resp)); // Safe cast if data is at start
    net_zcp_release_rx(p);

    *ticket_out = resp.ticket_number;
    return true;
}

static void wait_for_office(int user_id, sim_shm_t *shm, volatile _Atomic bool *should_continue) {
    int d, h, m;
    int last_logged_hour = -1;
    pthread_mutex_lock(&shm->time_control.mutex);
    while (should_continue && atomic_load(should_continue) && !g_proc_shutdown) {
        sim_client_read_time(shm, &d, &h, &m);
        if (h >= 8 && h < 17) break;

        if (h != last_logged_hour) {
            LOG_DEBUG("User %d Waiting for Office (Time: %02d:%02d)", user_id, h, m);
            last_logged_hour = h;
        }

        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_nsec += 100000000; 
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }

        pthread_cond_timedwait(&shm->time_control.cond_tick, &shm->time_control.mutex, &ts);
    }
    pthread_mutex_unlock(&shm->time_control.mutex);
}

static void join_queue(int user_id, int service, uint32_t ticket, sim_shm_t *shm) {
    queue_status_t *q = &shm->queues[service];
    LOG_DEBUG("User %d joining queue %d [Ticket #%u]", user_id, service, ticket);
    atomic_fetch_add(&q->waiting_count, 1);

    unsigned int tail = atomic_fetch_add(&q->tail, 1);
    unsigned int idx = tail % 128; // Small ring buffer
    while (atomic_load(&q->tickets[idx]) != 0) usleep(100);

    atomic_store(&q->tickets[idx], ticket + 1); 

    pthread_mutex_lock(&q->mutex);
    pthread_cond_signal(&q->cond_added);
    pthread_mutex_unlock(&q->mutex);
}

static bool wait_service(int user_id, uint32_t ticket, int service, sim_shm_t *shm, volatile _Atomic bool *should_continue) {
    queue_status_t *q = &shm->queues[service];
    bool done = false;
    int d, h, m;

    pthread_mutex_lock(&q->mutex);
    while (should_continue && atomic_load(should_continue) && !g_proc_shutdown) {
        if (!atomic_load(&shm->time_control.sim_active)) break;

        if (atomic_load(&q->last_finished_ticket) >= ticket) {
            sim_client_read_time(shm, &d, &h, &m);
            LOG_DEBUG("[Day %d %02d:%02d] User %d Finished.", d, h, m, user_id);
            done = true;
            break;
        }

        // Check for Office Closing (Kickout)
        sim_client_read_time(shm, &d, &h, &m);
        if (h >= 17) {
            LOG_WARN("[Day %d %02d:%02d] User %d Kicked out (Office Closed).", d, h, m, user_id);
            done = false; 
            break; 
        }

        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_nsec += 100000000; 
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }

        pthread_cond_timedwait(&q->cond_served, &q->mutex, &ts);
    }
    pthread_mutex_unlock(&q->mutex);
    return done;
}

int run_user_simulation_loop(int user_id, int service_type, sim_shm_t *shm, volatile _Atomic bool *should_continue_flag) {
    if (!shm) return 1;

    char *reqs = getenv("PO_USER_REQUESTS");
    int count = (reqs && atoi(reqs) > 0) ? atoi(reqs) : 1;

    int d, h, m;
    sim_client_read_time(shm, &d, &h, &m);
    LOG_INFO("User %d Active (Requests: %d)", user_id, count);

    for (int i=0; i<count; i++) {
        LOG_DEBUG("User %d starting request iteration %d", user_id, i);

        if (should_continue_flag && !atomic_load(should_continue_flag)) {
            LOG_INFO("User %d exiting: should_continue_flag is FALSE", user_id);
            break;
        }
        if (g_proc_shutdown) {
            LOG_INFO("User %d exiting: g_proc_shutdown is TRUE", user_id);
            break;
        }

        if (i > 0 && shm->params.tick_nanos > 0) usleep(200000); 

        // 1. Wait for Office Hours (08:00 - 17:00)
        // If closed, this blocks until 08:00 the next day.
        wait_for_office(user_id, shm, should_continue_flag);
        
        int hr,mn;
         sim_client_read_time(shm, &d, &hr, &mn);
        LOG_INFO("User %d Entering Office (Hour: %d)", user_id, hr);

        // 2. Obtain Ticket
        uint32_t t = 0;
        LOG_DEBUG("User %d attempting to obtain ticket", user_id);

        // NOTE: obtain_ticket could fail if we are disconnected or issuer is busy
        // Also if we are at 16:59, we might get a ticket and then get kicked out.
        if (!obtain_ticket(shm, service_type, should_continue_flag, &t)) {
            LOG_WARN("User %d failed to obtain ticket, retrying/skipping", user_id);
            usleep(100000); 
            continue;
        }

        LOG_INFO("User %d obtained ticket #%u (Service Type: %d)", user_id, t, service_type);

        // 3. Join Queue
        join_queue(user_id, service_type, t, shm);
        LOG_DEBUG("User %d Joined Queue %d [Ticket #%u]", user_id, service_type, t);

        // 4. Wait Service
        if (wait_service(user_id, t, service_type, shm, should_continue_flag)) {
            LOG_INFO("User %d Service Complete [Ticket #%u]", user_id, t);
        } else {
            LOG_ERROR("User %d Service Interrupted/Failed [Ticket #%u]", user_id, t);
        }
    }
    LOG_INFO("User %d simulation loop complete", user_id);
    return 0;
}
