/* Minimal control bridge implementation.
 * Listens on a UNIX domain socket and accepts simple line-based commands.
 * For now commands are logged and acknowledged. This is a skeleton to be
 * expanded with proper framing/codec (bridge_codec.h) and Director API
 * dispatching.
 */

#define _POSIX_C_SOURCE 200809L
#include "bridge_mainloop.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Default control socket path. Adjustable later or moved to config. */
static const char* ctrl_socket_path = "/tmp/post_office_ctrl.sock";

static int g_listen_fd = -1;
static volatile sig_atomic_t g_bridge_running = 0;

static void handle_client_fd(int client_fd) {
    FILE* f = fdopen(client_fd, "r+");
    if (!f) {
        close(client_fd);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        /* Trim newline */
        size_t n = strlen(line);
        if (n && line[n-1] == '\n') line[n-1] = '\0';

        /* Basic dispatch: for now just log and reply OK */
        fprintf(stdout, "ctrl-bridge: received command: %s\n", line);
        fflush(stdout);

        /* TODO: integrate with Director APIs (spawn users, query stats, etc.) */

        fprintf(f, "OK\n");
        fflush(f);
    }

    fclose(f); /* closes client_fd */
}

static void* client_thread_main(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);
    handle_client_fd(client_fd);
    return NULL;
}

int bridge_mainloop_init(void) {
    struct sockaddr_un addr;

    /* Remove existing socket if present */
    unlink(ctrl_socket_path);

    g_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        fprintf(stderr, "bridge: socket() failed: %s\n", strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ctrl_socket_path, sizeof(addr.sun_path) - 1);

    if (bind(g_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "bridge: bind() failed: %s\n", strerror(errno));
        close(g_listen_fd);
        g_listen_fd = -1;
        return -1;
    }

    if (listen(g_listen_fd, 4) != 0) {
        fprintf(stderr, "bridge: listen() failed: %s\n", strerror(errno));
        close(g_listen_fd);
        g_listen_fd = -1;
        unlink(ctrl_socket_path);
        return -1;
    }

    /* Best-effort permissions: owner-only */
    chmod(ctrl_socket_path, 0600);

    return 0;
}

int bridge_mainloop_run(void) {
    if (g_listen_fd < 0) {
        fprintf(stderr, "bridge: not initialized\n");
        return -1;
    }

    g_bridge_running = 1;
    fprintf(stdout, "bridge: listening on %s\n", ctrl_socket_path);

    while (g_bridge_running) {
        int client_fd = accept(g_listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            /* If stopping, accept may fail after close; break cleanly */
            if (!g_bridge_running) break;
            fprintf(stderr, "bridge: accept() failed: %s\n", strerror(errno));
            /* sleep briefly to avoid busy loop on repeated errors */
            sleep(1);
            continue;
        }

        /* For simplicity handle connection in a detached thread so bridge
         * remains responsive. Connections are expected to be short-lived. */
        pthread_t t;
        int* pfd = malloc(sizeof(int));
        if (!pfd) {
            close(client_fd);
            continue;
        }
        *pfd = client_fd;

        int rc = pthread_create(&t, NULL, client_thread_main, pfd);
        if (rc != 0) {
            /* Fallback: handle inline */
            free(pfd);
            handle_client_fd(client_fd);
        } else {
            /* Detach thread: it will close the fd when done. */
            pthread_detach(t);
        }
    }

    return 0;
}

void bridge_mainloop_stop(void) {
    g_bridge_running = 0;
    if (g_listen_fd >= 0) {
        close(g_listen_fd);
        g_listen_fd = -1;
    }
    unlink(ctrl_socket_path);
}
