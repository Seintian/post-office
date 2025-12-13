/* Minimal control bridge implementation using postoffice/net API.
 * Listens on a UNIX domain socket and accepts simple line-based commands.
 */

#define _POSIX_C_SOURCE 200809L
#include "bridge_mainloop.h"
#include <postoffice/metrics/metrics.h>
#include <postoffice/log/logger.h>
#include <postoffice/net/net.h>
#include <postoffice/net/socket.h>
#include <postoffice/net/poller.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Default control socket path. */
static const char* ctrl_socket_path = "/tmp/post_office_ctrl.sock";

static volatile int g_bridge_running = 0;
static poller_t *g_poller = NULL;

static void handle_client_fd(int client_fd) {
    // For now, we still use stdio streams for line parsing simplicity in this bridge.
    // In a full implementation, we would use the poller for reading too.
    // But to respect the "detached thread" model or simple inline handling:
    
    // Set to blocking for stdio usage (po_socket_accept returns non-blocking)
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags & ~O_NONBLOCK);

    FILE* f = fdopen(client_fd, "r+");
    if (!f) {
        po_socket_close(client_fd);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        if (n && line[n-1] == '\n') line[n-1] = '\0';

        LOG_INFO("ctrl-bridge: received command: %s", line);
        PO_METRIC_COUNTER_INC("director.bridge.commands");

        /* TODO: integrate with Director APIs */

        fprintf(f, "OK\n");
        fflush(f);
    }

    fclose(f); // closes client_fd
}

int bridge_mainloop_init(void) {
    // Cleanup old socket
    unlink(ctrl_socket_path);

    // Initialize metrics
    PO_METRIC_COUNTER_CREATE("director.bridge.connections");
    PO_METRIC_COUNTER_CREATE("director.bridge.commands");

    return 0;
}

int bridge_mainloop_run(void) {
    // 1. Create Listening Socket
    int listen_fd = po_socket_listen_unix(ctrl_socket_path, 4);
    if (listen_fd < 0) {
        LOG_ERROR("bridge: listen failed: %s", strerror(errno));
        return -1;
    }
    
    // Best effort permissions
    chmod(ctrl_socket_path, 0600);

    // 2. Create Poller
    g_poller = poller_create();
    if (!g_poller) {
        LOG_ERROR("bridge: poller create failed");
        po_socket_close(listen_fd);
        return -1;
    }

    // 3. Register Listener
    if (poller_add(g_poller, listen_fd, EPOLLIN) != 0) {
        LOG_ERROR("bridge: poller add failed");
        poller_destroy(g_poller);
        g_poller = NULL;
        po_socket_close(listen_fd);
        return -1;
    }

    g_bridge_running = 1;
    LOG_INFO("bridge: listening on %s", ctrl_socket_path);

    struct epoll_event events[16];
    
    while (g_bridge_running) {
        // Wait for events (1s timeout to check g_bridge_running)
        int n = poller_wait(g_poller, events, 16, 1000);
        if (n < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("bridge: poller_wait failed: %s", strerror(errno));
            break; // Fatal error
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == listen_fd) {
                // Accept new connections
                char addr_buf[64]; // Not used for unix sockets really
                int client_fd = po_socket_accept(listen_fd, addr_buf, sizeof(addr_buf));
                
                if (client_fd >= 0) {
                    PO_METRIC_COUNTER_INC("director.bridge.connections");
                    // Handle client (blocking for now in separate thread or inline)
                    // We'll spawn a thread to keep the main loop responsive to new connections
                    pthread_t t;
                    int *pfd = malloc(sizeof(int));
                    if (pfd) {
                        *pfd = client_fd;
                        void* client_thread(void* arg) {
                            int cfd = *(int*)arg;
                            free(arg);
                            handle_client_fd(cfd);
                            return NULL;
                        }
                        if (pthread_create(&t, NULL, client_thread, pfd) == 0) {
                            pthread_detach(t);
                        } else {
                            free(pfd);
                            po_socket_close(client_fd);
                        }
                    } else {
                        po_socket_close(client_fd);
                    }
                }
            }
        }
    }

    poller_destroy(g_poller);
    g_poller = NULL;
    po_socket_close(listen_fd);
    unlink(ctrl_socket_path);
    return 0;
}

void bridge_mainloop_stop(void) {
    g_bridge_running = 0;
    if (g_poller) {
        poller_wake(g_poller);
    }
}
