/**
 * @file poller.c
 * @brief Epoll-based poller simple wrapper.
 */

#include "net/poller.h"
#include "metrics/metrics.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct poller {
    int epfd;
};

poller_t *poller_create(void) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        PO_METRIC_COUNTER_INC("poller.create.fail");
        return NULL;
    }

    poller_t *p = (poller_t *)calloc(1, sizeof(*p));
    if (!p) {
        close(epfd);
        return NULL;
    }

    p->epfd = epfd;
    PO_METRIC_COUNTER_INC("poller.create");
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

    if (epoll_ctl(p->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        PO_METRIC_COUNTER_INC("poller.add.fail");
        return -1;
    }
    PO_METRIC_COUNTER_INC("poller.add");
    return 0;
}

int poller_mod(const poller_t *p, int fd, uint32_t events) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(p->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        PO_METRIC_COUNTER_INC("poller.mod.fail");
        return -1;
    }
    PO_METRIC_COUNTER_INC("poller.mod");
    return 0;
}

int poller_remove(const poller_t *p, int fd) {
    if (epoll_ctl(p->epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        PO_METRIC_COUNTER_INC("poller.del.fail");
        return -1;
    }
    PO_METRIC_COUNTER_INC("poller.del");
    return 0;
}

int poller_wait(const poller_t *p, struct epoll_event *events, int max_events, int timeout) {
    if (max_events <= 0) {
        errno = EINVAL;
        return -1;
    }

    int n = epoll_wait(p->epfd, events, max_events, timeout);
    if (n < 0) {
        if (errno == EINTR) {
            PO_METRIC_COUNTER_INC("poller.wait.eintr");
            return 0; // interrupted, no events
        }
        PO_METRIC_COUNTER_INC("poller.wait.fail");
        return -1;
    }
    if (n == 0) {
        PO_METRIC_COUNTER_INC("poller.wait.timeout");
        return 0;
    }
    PO_METRIC_COUNTER_INC("poller.wait");
    PO_METRIC_COUNTER_ADD("poller.wait.events", n);
    return n;
}

/* poller wake functionality not implemented here; instrumentation placeholder removed */
