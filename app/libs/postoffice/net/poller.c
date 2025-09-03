/**
 * @file poller.c
 * @brief Epoll-based poller simple wrapper.
 */

#include "net/poller.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct poller {
    int epfd;
};

poller_t *poller_create(void) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0)
        return NULL;
    poller_t *p = (poller_t *)calloc(1, sizeof(*p));
    if (!p) {
        close(epfd);
        return NULL;
    }
    p->epfd = epfd;
    return p;
}

void poller_destroy(poller_t *p) {
    close(p->epfd);
    free(p);
}

int poller_add(const poller_t *p, int fd, uint32_t events) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;
    return epoll_ctl(p->epfd, EPOLL_CTL_ADD, fd, &ev);
}

int poller_mod(const poller_t *p, int fd, uint32_t events) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;
    return epoll_ctl(p->epfd, EPOLL_CTL_MOD, fd, &ev);
}

int poller_remove(const poller_t *p, int fd) { return epoll_ctl(p->epfd, EPOLL_CTL_DEL, fd, NULL); }

int poller_wait(const poller_t *p, struct epoll_event *events, int max_events, int timeout) {
    if (max_events <= 0) {
        errno = EINVAL;
        return -1;
    }
    return epoll_wait(p->epfd, events, max_events, timeout);
}
