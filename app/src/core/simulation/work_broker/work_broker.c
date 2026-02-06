/**
 * @file work_broker.c
 * @brief Work Broker Process Implementation.
 * Manages Priority Queues for User Requests.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <postoffice/concurrency/threadpool.h>
#include <postoffice/log/logger.h>
#include <postoffice/net/net.h>
#include <postoffice/net/poller.h>
#include <postoffice/net/socket.h>
#include <postoffice/priority_queue/priority_queue.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "api/broker_handler.h"
#include "ipc/sim_client.h"
#include "ipc/simulation_ipc.h"

// --- Globals ---
static volatile sig_atomic_t g_shutdown = 0;
po_priority_queue_t *g_queues[SIM_MAX_SERVICE_TYPES];
pthread_mutex_t g_queue_mutexes[SIM_MAX_SERVICE_TYPES];

static void on_sig(int s, siginfo_t *i, void *c) {
    (void)s;
    (void)i;
    (void)c;
    g_shutdown = 1;
}

typedef struct {
    int client_fd;
    sim_shm_t *shm;
} task_ctx_t;

static void worker_task(void *arg) {
    task_ctx_t *ctx = (task_ctx_t *)arg;
    broker_handler_process_request(ctx->client_fd, ctx->shm);
    free(ctx);
}

// Helpers for Priority Queue (matching logic from memory)
static unsigned long item_hash(const void *ptr) {
    return (unsigned long)(uintptr_t)ptr;
}

// Comparator: Returns < 0 if a < b (Higher Priority).
// Duplicating structure here for convenience in comparison logic or using a shared header would be
// better. But for this refactor, let's keep it consistent with the handler.
typedef struct {
    uint32_t ticket_number;
    int is_vip;
    pid_t requester_pid;
    struct timespec arrival_time;
} broker_item_t;

static int item_compare(const void *a, const void *b) {
    const broker_item_t *ia = (const broker_item_t *)a;
    const broker_item_t *ib = (const broker_item_t *)b;
    if (ia->is_vip != ib->is_vip)
        return ib->is_vip - ia->is_vip;
    if (ia->arrival_time.tv_sec != ib->arrival_time.tv_sec)
        return (int)(ia->arrival_time.tv_sec - ib->arrival_time.tv_sec);
    return (int)(ia->arrival_time.tv_nsec - ib->arrival_time.tv_nsec);
}

int main(int argc, char *argv[]) {
    po_sysinfo_t sysinfo;
    po_sysinfo_collect(&sysinfo);

    char *loglevel = "INFO";
    size_t pool_size = 0;

    struct option long_opts[] = {{"pool-size", required_argument, 0, 'p'},
                                 {"loglevel", required_argument, 0, 'l'},
                                 {0, 0, 0, 0}};
    int opt;
    while ((opt = getopt_long(argc, argv, "l:p:", long_opts, NULL)) != -1) {
        if (opt == 'p')
            pool_size = (size_t)atol(optarg);
        if (opt == 'l')
            loglevel = optarg;
    }

    if (pool_size == 0) {
        pool_size = (size_t)sysinfo.physical_cores * 4;
        if (pool_size < 32)
            pool_size = 32;
    }

    if (po_logger_init(&(po_logger_config_t){.level = po_logger_level_from_str(loglevel) == -1
                                                          ? LOG_INFO
                                                          : po_logger_level_from_str(loglevel),
                                             .ring_capacity = 4096,
                                             .consumers = 1}) != 0)
        return 1;
    po_logger_add_sink_file("logs/work_broker.log", true);

    for (int i = 0; i < SIM_MAX_SERVICE_TYPES; i++) {
        g_queues[i] = po_priority_queue_create(item_compare, item_hash);
        pthread_mutex_init(&g_queue_mutexes[i], NULL);
    }

    sim_shm_t *shm = sim_ipc_shm_attach();
    if (!shm)
        return 1;

    sim_client_setup_signals(on_sig);

    if (net_init_zerocopy(128, 128, 4096) != 0)
        return 1;

    const char *user_home = getenv("HOME");
    char sock_path[1024];
    snprintf(sock_path, sizeof(sock_path), "%s/.postoffice/issuer.sock",
             user_home ? user_home : "/tmp");

    unlink(sock_path);
    int fd = po_socket_listen_unix(sock_path, 128);
    if (fd < 0) {
        LOG_FATAL("Work Broker: Failed to bind socket %s", sock_path);
        return 1;
    }
    LOG_INFO("Work Broker Started on %s", sock_path);

    threadpool_t *tp = tp_create(pool_size, 4096);
    atomic_fetch_add(&shm->stats.connected_threads, (uint32_t)pool_size + 1);
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

    LOG_INFO("Work Broker Shutting Down...");
    atomic_fetch_sub(&shm->stats.active_threads, 1);
    atomic_fetch_sub(&shm->stats.connected_threads, (uint32_t)pool_size + 1);

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
