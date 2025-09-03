/**
 * @file poller.h
 * @brief Event polling (epoll) abstraction interface.
 * @details Provides an epoll-based poller to monitor multiple file descriptors
 * for I/O readiness. Uses edge-triggered notifications (with EPOLLONESHOT
 * for one-shot rearm as needed).
 * @ingroup net
 */
#ifndef _POLLER_H
#define _POLLER_H

#include <sys/epoll.h>
#include <stdint.h>
#include <sys/cdefs.h>

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
void poller_destroy(poller_t *poller) __nonnull((1));

/**
 * @brief Add a file descriptor to the poller.
 * @param poller The poller instance.
 * @param fd File descriptor to add.
 * @param events EPOLL event flags (e.g., EPOLLIN, EPOLLOUT).
 * @return 0 on success, or -1 on error.
 */
int poller_add(const poller_t *poller, int fd, uint32_t events) __nonnull((1));

/**
 * @brief Modify the events for a registered file descriptor.
 * @param poller The poller instance.
 * @param fd File descriptor to modify.
 * @param events New EPOLL event flags.
 * @return 0 on success, or -1 on error.
 */
int poller_mod(const poller_t *poller, int fd, uint32_t events) __nonnull((1));

/**
 * @brief Remove a file descriptor from the poller.
 * @param poller The poller instance.
 * @param fd File descriptor to remove.
 * @return 0 on success, or -1 on error.
 */
int poller_remove(const poller_t *poller, int fd) __nonnull((1));

/**
 * @brief Wait for events on the poller.
 * @param poller The poller instance.
 * @param events Array of epoll_event to populate with ready events.
 * @param max_events Maximum number of events to retrieve.
 * @param timeout Maximum time to wait (milliseconds, -1 for infinite).
 * @return Number of events returned, or -1 on error.
 */
int poller_wait(const poller_t *poller, struct epoll_event *events, int max_events, int timeout)
    __nonnull((1,2));

#endif // _POLLER_H
