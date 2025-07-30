#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "net/poller.h"
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>
#include <stdatomic.h>

struct poller {
    int               epfd;       // epoll instance fd
    perf_ringbuf_t   *queue;     // ring buffer of poller_event_t*
    perf_zcpool_t    *pool;      // pool for event objects
    size_t            max_events;
};

poller_t *poller_new(size_t max_events, perf_zcpool_t *pool) {
    if ((max_events & (max_events - 1)) != 0) {
        errno = EINVAL;
        return NULL;
    }

    poller_t *p = calloc(1, sizeof(*p));
    if (!p)
        return NULL;

    p->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (p->epfd < 0) {
        free(p);
        return NULL;
    }

    p->queue      = perf_ringbuf_create(max_events);
    p->pool       = pool;
    p->max_events = max_events;
    if (!p->queue) {
        close(p->epfd);
        free(p);
        errno = ENOMEM;
        return NULL;
    }

    return p;
}

int poller_ctl(
    const poller_t *p,
    int             fd,
    uint32_t        events,
    void           *user,
    int             op
) {
    if (fd < 0 || !(op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD || op == EPOLL_CTL_DEL)) {
        errno = EINVAL;
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events   = events;
    ev.data.ptr = user;

    if (epoll_ctl(p->epfd, op, fd, &ev) < 0)
        return -1;

    return 0;
}

int poller_wait(poller_t *p, int timeout_ms) {
    struct epoll_event *events = malloc(sizeof(*events) * p->max_events);
    if (!events)
        return -1;

    int n = epoll_wait(p->epfd, events, (int)p->max_events, timeout_ms);
    if (n < 0) {
        free(events);
        return -1;
    }

    int enqueued = 0;
    for (int i = 0; i < n; i++) {
        poller_event_t *e = perf_zcpool_acquire(p->pool);
        if (!e) {
            // pool exhausted: stop queuing
            break;
        }

        e->fd        = events[i].data.fd >= 0
                        ? events[i].data.fd
                        : -1;
        e->events    = events[i].events;
        e->user_data = events[i].data.ptr;

        if (perf_ringbuf_enqueue(p->queue, e) < 0) {
            // queue full: release and stop
            perf_zcpool_release(p->pool, e);
            break;
        }

        enqueued++;
    }

    free(events);
    return enqueued;
}

int poller_next(poller_t *p, poller_event_t **ev) {
    void *raw;
    int rc = perf_ringbuf_dequeue(p->queue, &raw);
    if (rc < 0)
        return 0;

    *ev = (poller_event_t*)raw;
    return 1;
}

void poller_free(poller_t **pp) {
    if (!*pp)
        return;

    poller_t *p = *pp;
    close(p->epfd);
    perf_ringbuf_destroy(&p->queue);
    free(p);
    *pp = NULL;
}
