/**
 * @file ticket_issuer.c
 * @brief Ticket Issuer Process Implementation.
 */

#define _POSIX_C_SOURCE 200809L

#include "api/ticket_handler.h"
#include "ipc/sim_client.h"
#include "ipc/simulation_ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <postoffice/concurrency/threadpool.h>
#include <postoffice/net/net.h>
#include <postoffice/net/poller.h>
#include <postoffice/log/logger.h>
#include <postoffice/net/socket.h>
#include <errno.h>
#include <postoffice/sysinfo/sysinfo.h> // Sysinfo integration

static volatile sig_atomic_t g_shutdown = 0;
static void on_sig(int s, siginfo_t* i, void* c) {
    (void)s; (void)i; (void)c;
    g_shutdown = 1;
}

typedef struct {
    int client_fd;
    sim_shm_t *shm;
} task_ctx_t;

static void worker_task(void *arg) {
    task_ctx_t *ctx = (task_ctx_t*)arg;
    int client_fd = ctx->client_fd;  // Save for logging after free
    
    LOG_DEBUG("worker_task started: client_fd=%d", client_fd);
    ticket_handler_process_request(ctx->client_fd, ctx->shm);
    
    LOG_DEBUG("worker_task completed: client_fd=%d", client_fd);
    free(ctx);
}

int main(int argc, char *argv[]) {
    po_sysinfo_t sysinfo;
    po_sysinfo_collect(&sysinfo);

    // 1. Args
    char *loglevel = "INFO";
    size_t pool_size = 0; // Default to dynamic

    struct option long_opts[] = {
        {"pool-size", required_argument, 0, 'p'},
        {"loglevel", required_argument, 0, 'l'},
        {"l", required_argument, 0, 'l'},
        {0, 0, 0, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "l:", long_opts, NULL)) != -1) {
        if (opt == 'p') pool_size = (size_t)atol(optarg);
        if (opt == 'l') loglevel = optarg;
    }

    if (pool_size == 0) {
        // Dynamic sizing: 4x cores, min 32
        pool_size = (size_t)sysinfo.physical_cores * 4;
        if (pool_size < 32) pool_size = 32;
    }

    // 2. Init
    if (po_logger_init(&(po_logger_config_t){
        .level = po_logger_level_from_str(loglevel) == -1 ? LOG_INFO : po_logger_level_from_str(loglevel),
        .ring_capacity = 4096,
        .consumers = 1
    }) != 0) return 1;
    po_logger_add_sink_file("logs/ticket_issuer.log", false);
    // po_logger_add_sink_console(false);

    sim_shm_t *shm = sim_ipc_shm_attach();
    if (!shm) return 1;

    sim_client_setup_signals(on_sig);

    if (net_init_zerocopy(128, 128, 4096) != 0) {
        LOG_FATAL("Net Init Failed");
        return 1;
    }

    // 3. Create socket (same pattern as Director)
    const char *user_home = getenv("HOME");
    char sock_path[512];
    if (user_home) {
        snprintf(sock_path, sizeof(sock_path), "%s/.postoffice/issuer.sock", user_home);
    } else {
        snprintf(sock_path, sizeof(sock_path), "/tmp/postoffice_%d_issuer.sock", getuid());
    }

    unlink(sock_path);
    int fd = po_socket_listen_unix(sock_path, 128);
    if (fd < 0) {
        LOG_FATAL("Failed to bind Ticket Issuer socket: %s", sock_path);
        return 1;
    }
    LOG_INFO("Ticket Issuer: Created and listening on %s (fd=%d)", sock_path, fd);

    threadpool_t *tp = tp_create(pool_size, 4096);
    if (!tp) {
        LOG_FATAL("Failed to create thread pool");
        return 1;
    }
    atomic_fetch_add(&shm->stats.connected_threads, (uint32_t)pool_size + 1);
    atomic_fetch_add(&shm->stats.active_threads, 1); // Main thread is active
    tp_set_active_counter(tp, &shm->stats.active_threads);
    poller_t *poller = poller_create();
    poller_add(poller, fd, EPOLLIN);

    LOG_INFO("Ticket Issuer Started (FD=%d, Pool=%zu threads, Queue=4096)", fd, pool_size);

    int last_day = 0;
    struct epoll_event ev[32];
    int poll_count = 0;

    while (!g_shutdown) {
        // Accept connections during simulation time (non-blocking poll)
        int n = poller_wait(poller, ev, 32, 100);
        poll_count++;

        if (poll_count <= 100 || poll_count % 100 == 0) {
            LOG_TRACE("Ticket Issuer: Poll #%d returned %d events", poll_count, n);
        }

        if (n < 0 && errno != EINTR) break;

        for (int i=0; i<n; i++) {
            if (ev[i].data.fd == fd) {
                LOG_INFO("Ticket Issuer: Accepting connections on FD %d", fd);
                while (!g_shutdown) {
                    int client = po_socket_accept(fd, NULL, 0);
                    if (client < 0) break;

                    LOG_DEBUG("Ticket Issuer: Accepted connection (fd=%d)", client);

                    task_ctx_t *ctx = malloc(sizeof(task_ctx_t));
                    if (ctx) {
                        ctx->client_fd = client;
                        ctx->shm = shm;
                        LOG_DEBUG("Submitting client_fd=%d to thread pool", client);
                        int ret = tp_submit(tp, worker_task, ctx);
                        if (ret != 0) {
                            LOG_ERROR("Failed to submit client_fd=%d to thread pool (queue full?)", client);
                            free(ctx);
                            po_socket_close(client);
                        } else {
                            LOG_DEBUG("Submitted client_fd=%d to thread pool", client);
                        }
                    } else {
                        LOG_ERROR("Ticket Issuer: Failed to allocate task context");
                        po_socket_close(client);
                    }
                }
            }
        }

        // Check for end-of-day barrier synchronization
        if (atomic_load(&shm->sync.barrier_active)) {
            sim_client_wait_barrier(shm, &last_day, &g_shutdown);
        }
    }

    LOG_INFO("Ticket Issuer Shutting Down...");
    atomic_fetch_sub(&shm->stats.active_threads, 1);
    atomic_fetch_sub(&shm->stats.connected_threads, (uint32_t)pool_size + 1);
    tp_destroy(tp, true);
    poller_destroy(poller);
    net_shutdown_zerocopy();
    sim_ipc_shm_detach(shm);
    po_logger_shutdown();
    return 0;
}
