#define _POSIX_C_SOURCE 200809L

#include "api/broker_core.h"

#include <errno.h>
#include <postoffice/net/net.h>
#include <postoffice/net/socket.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "api/broker_handler.h"
#include "ipc/sim_client.h"

// --- Helpers ---

static unsigned long item_hash(const void *ptr) {
    return (unsigned long)(uintptr_t)ptr;
}

static int item_compare(const void *a, const void *b) {
    const broker_item_t *ia = (const broker_item_t *)a;
    const broker_item_t *ib = (const broker_item_t *)b;
    if (ia->is_vip != ib->is_vip)
        return ib->is_vip - ia->is_vip;
    if (ia->arrival_time.tv_sec != ib->arrival_time.tv_sec)
        return (int)(ia->arrival_time.tv_sec - ib->arrival_time.tv_sec);
    return (int)(ia->arrival_time.tv_nsec - ib->arrival_time.tv_nsec);
}

typedef struct {
    int client_fd;
    broker_ctx_t *ctx;
} task_data_t;

static void worker_task(void *arg) {
    task_data_t *data = (task_data_t *)arg;
    broker_handler_process_request(data->client_fd, data->ctx);
    free(data);
}

// --- Implementation ---

int broker_init(broker_ctx_t *ctx, const char *loglevel, size_t pool_size) {
    if (!ctx)
        return -1;
    memset(ctx, 0, sizeof(*ctx));
    ctx->pool_size = pool_size;
    ctx->shutdown_requested = 0;

    // Logger
    po_logger_config_t log_cfg = {.level = po_logger_level_from_str(loglevel) == -1
                                               ? LOG_INFO
                                               : po_logger_level_from_str(loglevel),
                                  .ring_capacity = 4096,
                                  .consumers = 1};
    if (po_logger_init(&log_cfg) != 0)
        return -1;
    po_logger_add_sink_file("logs/work_broker.log", true);

    // Queues
    for (int i = 0; i < SIM_MAX_SERVICE_TYPES; i++) {
        ctx->queues[i] = po_priority_queue_create(item_compare, item_hash);
        pthread_mutex_init(&ctx->queue_mutexes[i], NULL);
    }

    // SHM
    ctx->shm = sim_ipc_shm_attach();
    if (!ctx->shm)
        return -1;

    // Net
    if (net_init_zerocopy(128, 128, 4096) != 0)
        return -1;

    const char *user_home = getenv("HOME");
    char sock_path[1024];
    snprintf(sock_path, sizeof(sock_path), "%s/.postoffice/issuer.sock",
             user_home ? user_home : "/tmp");

    unlink(sock_path);
    ctx->socket_fd = po_socket_listen_unix(sock_path, 128);
    if (ctx->socket_fd < 0) {
        LOG_FATAL("Work Broker: Failed to bind socket %s", sock_path);
        return -1;
    }
    LOG_INFO("Work Broker Started on %s", sock_path);

    // Threadpool
    ctx->tp = tp_create(pool_size, 4096);
    if (!ctx->tp)
        return -1;

    atomic_fetch_add(&ctx->shm->stats.connected_threads, (uint32_t)pool_size + 1);
    atomic_fetch_add(&ctx->shm->stats.active_threads, 1);
    tp_set_active_counter(ctx->tp, &ctx->shm->stats.active_threads);

    // Poller
    ctx->poller = poller_create();
    poller_add(ctx->poller, ctx->socket_fd, EPOLLIN);

    return 0;
}

void broker_run(broker_ctx_t *ctx) {
    struct epoll_event ev[32];
    int last_day = 0;

    while (!ctx->shutdown_requested) {
        int n = poller_wait(ctx->poller, ev, 32, 100);
        if (n < 0 && errno != EINTR)
            break;

        for (int i = 0; i < n; i++) {
            if (ev[i].data.fd == ctx->socket_fd) {
                while (!ctx->shutdown_requested) {
                    int client = po_socket_accept(ctx->socket_fd, NULL, 0);
                    if (client < 0)
                        break;

                    task_data_t *data = malloc(sizeof(task_data_t));
                    data->client_fd = client;
                    data->ctx = ctx; // Pass context

                    tp_submit(ctx->tp, worker_task, data);
                }
            }
        }

        if (atomic_load(&ctx->shm->sync.barrier_active)) {
            // Cast generic shutdown flag locally if needed or update sim_client_wait_barrier sig
            // checking sim_client definition for wait_barrier signature: (shm, last_day,
            // shutdown_ptr) shutdown_ptr is volatile sig_atomic_t* in original code. We cast our
            // volatile sig_atomic_t* to expected type if same. or create a temp pointer if strict
            // aliasing complains, but it should be fine.
            sim_client_wait_barrier(ctx->shm, &last_day,
                                    (volatile sig_atomic_t *)&ctx->shutdown_requested);
        }
    }
}

void broker_shutdown(broker_ctx_t *ctx) {
    LOG_INFO("Work Broker Shutting Down...");

    if (ctx->shm) {
        atomic_fetch_sub(&ctx->shm->stats.active_threads, 1);
        atomic_fetch_sub(&ctx->shm->stats.connected_threads, (uint32_t)ctx->pool_size + 1);
    }

    if (ctx->tp)
        tp_destroy(ctx->tp, true);
    if (ctx->poller)
        poller_destroy(ctx->poller);

    net_shutdown_zerocopy();
    if (ctx->shm)
        sim_ipc_shm_detach(ctx->shm);

    for (int i = 0; i < SIM_MAX_SERVICE_TYPES; i++) {
        if (ctx->queues[i])
            po_priority_queue_destroy(ctx->queues[i]);
        pthread_mutex_destroy(&ctx->queue_mutexes[i]);
    }

    po_logger_shutdown();
}
