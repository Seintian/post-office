/**
 * @file poller.c
 * @brief Epoll-based poller simple wrapper.
 */

#include "net/poller.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

#include "metrics/metrics.h"

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME ((clockid_t)0)
#endif

struct poller {
    int epfd;
    int efd; // eventfd for wakeups
};

poller_t *poller_create(void) {
    poller_t *p = calloc(1, sizeof(*p));
    if (!p)
        return NULL;

    p->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (p->epfd < 0) {
        PO_METRIC_COUNTER_INC("poller.create.fail");
        free(p);
        return NULL;
    }
    p->efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (p->efd < 0) {
        PO_METRIC_COUNTER_INC("poller.create.fail");
        close(p->epfd);
        free(p);
        return NULL;
    }

    struct epoll_event ev = {
        .events = EPOLLIN,
        .data.fd = p->efd
    };
    if (epoll_ctl(p->epfd, EPOLL_CTL_ADD, p->efd, &ev) < 0) {
        PO_METRIC_COUNTER_INC("poller.create.fail");

        close(p->efd);
        close(p->epfd);
        free(p);
        return NULL;
    }

    PO_METRIC_COUNTER_INC("poller.create");
    return p;
}

void poller_destroy(poller_t *p) {
    if (p->efd >= 0)
        close(p->efd);

    if (p->epfd >= 0)
        close(p->epfd);

    free(p);
}

int poller_add(const poller_t *p, int fd, uint32_t events) {
    struct epoll_event ev = {
        .events = events,
        .data.fd = fd
    };

    if (epoll_ctl(p->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        PO_METRIC_COUNTER_INC("poller.add.fail");
        return -1;
    }

    PO_METRIC_COUNTER_INC("poller.add");
    return 0;
}

int poller_mod(const poller_t *p, int fd, uint32_t events) {
    struct epoll_event ev = {
        .events = events,
        .data.fd = fd
    };

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

    // Filter out internal wake fd events in-place.
    int write_idx = 0;
    for (int i = 0; i < n; ++i) {
        if (events[i].data.fd == p->efd) {
            // drain eventfd (value may coalesce)
            uint64_t val;
            ssize_t r = read(p->efd, &val, sizeof(val));
            (void)r; // ignore partial; wake semantics satisfied
            PO_METRIC_COUNTER_INC("poller.wake");
            continue; // skip exposing
        }

        if (write_idx != i)
            events[write_idx] = events[i];

        write_idx++;
    }
    if (write_idx == 0) {
        // only wake events
        return 0;
    }

    PO_METRIC_COUNTER_INC("poller.wait");
    PO_METRIC_COUNTER_ADD("poller.wait.events", write_idx);
    return write_idx;
}

int poller_wake(const poller_t *p) {
    uint64_t one = 1;
    if (write(p->efd, &one, sizeof(one)) != (ssize_t)sizeof(one)) {
        if (errno == EAGAIN) {
            // Saturated counter; still acts as a wake signal.
            return 0;
        }

        return -1;
    }

    return 0;
}

int poller_timed_wait(const poller_t *p, struct epoll_event *events, int max_events,
                      int total_timeout_ms, bool *timed_out) {
    if (timed_out)
        *timed_out = false;

    if (total_timeout_ms < 0) {
        // Delegate to regular wait (infinite) semantics.
        return poller_wait(p, events, max_events, -1);
    }

    if (total_timeout_ms == 0)
        return poller_wait(p, events, max_events, 0);

    struct timespec start, end;
    if (timed_out)
        *timed_out = false;

    if (clock_gettime(CLOCK_REALTIME, &start) != 0)
        return -1;

    int n = poller_wait(p, events, max_events, total_timeout_ms);
    if (clock_gettime(CLOCK_REALTIME, &end) != 0)
        return -1;

    if (n == 0 && timed_out) {
        long elapsed_ms =
            (end.tv_sec - start.tv_sec) * 1000L + (end.tv_nsec - start.tv_nsec) / 1000000L;
        if (elapsed_ms >= total_timeout_ms)
            *timed_out = true; // true timeout
    }
    return n;
}
