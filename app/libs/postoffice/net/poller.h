#ifndef _NET_POLLER_H
#define _NET_POLLER_H

#include <stdint.h>
#include <sys/epoll.h>
#include "perf/ringbuf.h"
#include "perf/zerocopy.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque poller handle
typedef struct poller poller_t;

/// One ready‐fd event, allocated from a zero‑copy pool.
typedef struct {
    uint32_t events;      ///< epoll events bitmask (EPOLLIN, EPOLLOUT, …)
    void    *user_data;   ///< custom pointer passed at registration
} poller_event_t;

/**
 * @brief Create a new poller.
 * 
 * @param max_events  Maximum number of simultaneous events (power‐of‐two).
 * @param pool        Zero‑copy pool for allocating `poller_event_t` objects.
 * @return Poller handle or NULL+errno.
 */
poller_t *poller_new(size_t max_events, perf_zcpool_t *pool) __nonnull((2));

/**
 * @brief Register or modify an fd to watch.
 * 
 * @param p        Poller
 * @param fd       File descriptor
 * @param events   EPOLLIN|EPOLLOUT|…
 * @param user     Arbitrary pointer to attach
 * @param op       Operation (EPOLL_CTL_ADD, EPOLL_CTL_MOD, EPOLL_CTL_DEL)
 * @return 0 on success, -1 on failure.
 */
int poller_ctl(
    const poller_t *p,
    int             fd,
    uint32_t        events,
    void           *user,
    int             op         /* EPOLL_CTL_ADD, MOD, DEL */
) __nonnull((1));

/**
 * @brief Wait for events.
 * 
 * Internally calls `epoll_wait(…, max_events, timeout_ms)` and
 * pushes each ready event into an internal ring buffer.
 * 
 * @param p           Poller
 * @param timeout_ms  Timeout in milliseconds (-1 = infinite)
 * @return Number of events queued, or -1 on error.
 */
int poller_wait(poller_t *p, int timeout_ms) __nonnull((1));

/**
 * @brief Retrieve next ready event (non‐blocking).
 * 
 * Pop one `poller_event_t*` from the internal queue.
 * Caller must call `perf_zcpool_release(pool, ev)` when done.
 * 
 * @param p     Poller
 * @param ev    Out pointer to event (owned by pool)
 * @return  1 on event, 0 if none left, -1 on error.
 */
int poller_next(poller_t *p, poller_event_t **ev) __nonnull((1, 2));

/**
 * @brief Destroy the poller, freeing all internal resources.
 * 
 * Does *not* free outstanding `poller_event_t` objects still in the queue.
 */
void poller_free(poller_t **p) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // _NET_POLLER_H
