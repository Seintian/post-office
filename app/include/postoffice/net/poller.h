/**
 * @file poller.h
 * @brief Epoll-based event loop abstraction.
 * @ingroup net
 */
#ifndef PO_NET_POLLER_H
#define PO_NET_POLLER_H

#include <stdint.h>
#include <sys/epoll.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration of the internal poller structure. */
typedef struct poller poller_t;

/**
 * @brief Create a new poller (wraps epoll_create).
 * @return A pointer to the new poller, or NULL on failure.
 */
poller_t *poller_create(void);

/**
 * @brief Destroy a poller and release its resources.
 * @param poller The poller to destroy.
 */
void poller_destroy(poller_t *poller);

/**
 * @brief Add a file descriptor to the poller.
 *
 * Recommended usage: register interest with EPOLLIN|EPOLLOUT selectively.
 * Avoid always enabling EPOLLOUT, as it is almost always ready and can cause
 * spurious wake-ups.
 * @param poller The poller instance.
 * @param fd File descriptor to add.
 * @param events EPOLL event flags (e.g., EPOLLIN, EPOLLOUT).
 * @return 0 on success, or -1 on error.
 */
int poller_add(const poller_t *poller, int fd, uint32_t events);

/**
 * @brief Modify the events for a registered file descriptor.
 *
 * Typically used after handling a one-shot event to rearm interest.
 * @param poller The poller instance.
 * @param fd File descriptor to modify.
 * @param events New EPOLL event flags.
 * @return 0 on success, or -1 on error.
 */
int poller_mod(const poller_t *poller, int fd, uint32_t events);

/**
 * @brief Remove a file descriptor from the poller.
 * @param poller The poller instance.
 * @param fd File descriptor to remove.
 * @return 0 on success, or -1 on error.
 */
int poller_remove(const poller_t *poller, int fd);

/**
 * @brief Wait for events on the poller.
 *
 * Caller must drain readiness (read/write loops) for edge-triggered FDs.
 * @param poller The poller instance.
 * @param events Array of epoll_event to populate with ready events.
 * @param max_events Maximum number of events to retrieve.
 * @param timeout Maximum time to wait (milliseconds, -1 for infinite).
 * @return Number of events returned, or -1 on error.
 */
int poller_wait(const poller_t *poller, struct epoll_event *events, int max_events, int timeout);

/**
 * @brief Wake any thread blocked in poller_wait().
 *
 * Internally uses an eventfd registered with epoll. The wake event is consumed
 * and filtered out before returning to the caller (thus it is not visible in
 * the returned events array). Safe to call from any thread.
 *
 * @return 0 on success, -1 on error (errno set).
 */
int poller_wake(const poller_t *poller);

/**
 * @brief Wait up to total_timeout_ms for events, supporting early wake.
 *
 * Implements a coarse timeout budget by subtracting elapsed time between
 * internal epoll_wait calls. Suitable for integrating periodic tasks.
 *
 * Repeatedly invokes poller_wait with the remaining time. Returns as soon as
 * at least one real event is available, on timeout, or if a wake is issued.
 *
 * @param poller  Poller instance.
 * @param events  Output array for events.
 * @param max_events Capacity of events array.
 * @param total_timeout_ms Total timeout budget in milliseconds (<=0 means immediate check).
 * @param timed_out Optional out flag set to true when the timeout fully elapsed (no wake/events).
 *                  Set to false when returning due to wake or real events.
 * @return number of events (>0), 0 on wake/timeout (check *timed_out), -1 on error.
 */
int poller_timed_wait(const poller_t *poller, struct epoll_event *events, int max_events,
                      int total_timeout_ms, bool *timed_out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // PO_NET_POLLER_H
