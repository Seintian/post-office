#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "net/socket.h"
#include "utils/errors.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>


static poller_t      *g_poller;
static perf_zcpool_t *g_evpool;

/// Helper: make socket nonblocking
static int _set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void po_set_event_cb(po_conn_t *c, po_event_cb cb, void *arg) {
    c->callback = cb;
    c->cb_arg = arg;

    // Register for EPOLLIN|EPOLLOUT events
    poller_ctl(g_poller, c->fd, EPOLLIN | EPOLLOUT, c, EPOLL_CTL_MOD);
}

int po_modify_events(po_conn_t *c, uint32_t events) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events   = events;
    ev.data.ptr = c;

    return poller_ctl(g_poller, c->fd, events, c, EPOLL_CTL_MOD);
}

/// Initialize the module
int sock_init(size_t max_events) {
    if (g_poller) {
        errno = NET_EALREADY;
        return -1;
    }

    // pool of poller_event_t objects
    g_evpool = perf_zcpool_create(max_events, sizeof(poller_event_t));
    if (!g_evpool)
        return -1;

    g_poller = poller_new(max_events, g_evpool);
    if (!g_poller) {
        perf_zcpool_destroy(&g_evpool);
        return -1;
    }

    return 0;
}

po_conn_t *sock_listen(const char *path, uint16_t port) {
    int fd = -1;
    if (strncmp(path, "unix:", 5) == 0) {
        // UNIX‐domain
        struct sockaddr_un addr = { .sun_family = AF_UNIX };
        strncpy(addr.sun_path, path + 5, sizeof(addr.sun_path) - 1);

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd >= 0) {
            unlink(addr.sun_path);
            if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0 ||
                listen(fd, SOMAXCONN) < 0)
                goto fail;
        }
    }
    else {
        // IPv4
        struct sockaddr_in in = {
            .sin_family = AF_INET,
            .sin_port   = htons(port),
            .sin_addr   = { .s_addr = inet_addr(path) }
        };
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            int on = 1;
            if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
                goto fail;

            if (bind(fd, (struct sockaddr*)&in, sizeof(in)) < 0 ||
                listen(fd, SOMAXCONN) < 0)
                goto fail;
        }
    }

    if (fd < 0)
        return NULL;

    if (_set_nonblocking(fd) < 0)
        goto fail;

    po_conn_t *c = malloc(sizeof(*c));
    if (!c)
        goto fail;

    c->fd = fd;

    // register for EPOLLIN
    if (poller_ctl(g_poller, fd, EPOLLIN, NULL, EPOLL_CTL_ADD) < 0){
        close(fd);
        free(c);
        return NULL;
    }
    return c;

fail:
    if (fd >= 0)
        close(fd);
    return NULL;
}

po_conn_t *sock_connect(const char *path, uint16_t port) {
    int fd = -1;
    if (strncmp(path, "unix:", 5) == 0) {
        struct sockaddr_un addr = { .sun_family = AF_UNIX };
        strncpy(addr.sun_path, path + 5, sizeof(addr.sun_path) - 1);

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd >= 0 && connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0
            && errno != EINPROGRESS)
            goto fail;
    }
    else {
        struct sockaddr_in in = {
            .sin_family = AF_INET,
            .sin_port   = htons(port),
            .sin_addr   = { .s_addr = inet_addr(path) }
        };
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0 && connect(fd, (struct sockaddr*)&in, sizeof(in)) < 0
            && errno != EINPROGRESS)
            goto fail;
    }

    if (fd < 0)
        return NULL;

    if (_set_nonblocking(fd) < 0)
        goto fail;

    po_conn_t *c = malloc(sizeof(*c));
    if (!c)
        goto fail;

    c->fd = fd;

    // register for both IN|OUT so caller sees connect completion
    if (poller_ctl(g_poller, fd, EPOLLIN|EPOLLOUT, c, EPOLL_CTL_ADD) < 0) {
        close(fd);
        free(c);
        return NULL;
    }

    return c;

fail:
    if (fd>=0)
        close(fd);
    return NULL;
}

int sock_accept(const po_conn_t *listener, po_conn_t **out) {
    int cfd = accept(listener->fd, NULL, NULL);
    if (cfd < 0)
        return -1;

    if (_set_nonblocking(cfd) < 0) {
        close(cfd);
        return -1;
    }

    po_conn_t *c = malloc(sizeof(*c));
    if (!c) {
        close(cfd);
        return -1;
    }

    c->fd = cfd;

    // register for read events
    if (poller_ctl(g_poller, cfd, EPOLLIN, c, EPOLL_CTL_ADD) < 0) {
        close(cfd);
        free(c);
        return -1;
    }

    *out = c;
    return 0;
}

void sock_close(po_conn_t **p) {
    if (!*p)
        return;

    po_conn_t *c = *p;
    poller_ctl(g_poller, c->fd, 0, NULL, EPOLL_CTL_DEL);
    close(c->fd);
    free(c);
    *p = NULL;
}

void sock_shutdown(void) {
    if (g_poller) {
        poller_free(&g_poller);
        g_poller = NULL;
    }

    if (g_evpool) {
        perf_zcpool_destroy(&g_evpool);
        g_evpool = NULL;
    }
}

ssize_t sock_send(const po_conn_t *c, const void *buf, size_t len) {
    return send(c->fd, buf, len, MSG_DONTWAIT|MSG_NOSIGNAL);
}

ssize_t sock_recv(const po_conn_t *c, void *buf, size_t len) {
    return recv(c->fd, buf, len, MSG_DONTWAIT);
}

poller_t *sock_poller(void) {
    return g_poller;
}

int sock_next_event(poller_event_t **ev) {
    return poller_next(g_poller, ev);
}
