#ifndef _NET_SOCKET_H
#define _NET_SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "net/poller.h"
#include "net/framing.h"
#include "perf/zerocopy.h"
#include "perf/batcher.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _po_conn_t po_conn_t;

/// Callback type for events on a connection
typedef void (*po_event_cb)(po_conn_t *c, uint32_t events, void *arg);

/// Connection handle (listening or established)
struct _po_conn_t {
    int fd;
    framing_decoder_t *decoder;
    perf_batcher_t    *batcher;
    perf_ringbuf_t *outq;
    po_event_cb callback;
    void *cb_arg;
};

/**
 * @brief Set a callback for connection events.
 * 
 * This will be called when the connection is ready for reading or writing,
 * or when an error occurs. The callback should handle the events and
 * perform necessary actions like reading/writing data.
 */
void po_set_event_cb(po_conn_t *c, po_event_cb cb, void *arg) __nonnull((1, 2));

/**
 * @brief Modify the events for a connection.
 * 
 * This allows changing the events that the poller will monitor for this connection.
 * 
 * @param c       The connection to modify.
 * @param events  The new events to monitor (EPOLLIN, EPOLLOUT, etc.).
 * @return 0 on success, -1 on error (errno set).
 */
int po_modify_events(po_conn_t *c, uint32_t events) __nonnull((1));

/**
 * @brief Initialize the socket subsystem.
 * 
 * Must be called once per process before any other po_* calls.
 * Internally creates a poller with capacity `max_events` and a
 * pool of `max_events` events.
 * 
 * @param max_events  Maximum simultaneous fds/events to track (power-of-two).
 * @return 0 on success, -1 on error (errno set).
 */
int sock_init(size_t max_events);

/**
 * @brief Bind & listen on either a Unix‐domain or IPv4/TCP socket.
 * 
 * If `path` starts with "unix:", the remainder is taken as the filesystem socket path.
 * Otherwise it's treated as an IPv4 literal (e.g. "127.0.0.1") and `port` is used.
 * 
 * The returned listener is nonblocking and registered for EPOLLIN.
 */
po_conn_t *sock_listen(const char *path, uint16_t port) __nonnull((1));

/**
 * @brief Connect to a remote listener (Unix or TCP).
 * 
 * Same `path` rules as sock_listen.  Returns a nonblocking socket
 * which you must then poll for EPOLLOUT to detect connect completion.
 */
po_conn_t *sock_connect(const char *path, uint16_t port) __nonnull((1));

/**
 * @brief Accept one incoming connection on a listening `po_conn_t`.
 * 
 * @param listener  The listening handle.
 * @param out       Receives the new connection handle (or NULL on error).
 * @return 0 on success, -1 on nonblocking "no connection" (errno = EAGAIN),  
 *         or -1 on fatal error (errno set).  
 */
int sock_accept(const po_conn_t *listener, po_conn_t **out) __nonnull((1, 2));

/**
 * @brief Close & destroy a connection (or listener).
 * 
 * Unregisters from the poller and closes the FD.
 */
void sock_close(po_conn_t **conn) __nonnull((1));

/**
 * @brief Shutdown the socket subsystem.
 * 
 * Cleans up the poller and zero-copy pool, freeing all resources.
 */
void sock_shutdown(void);

/**
 * @brief Send up to `len` bytes on this connection (nonblocking).
 * 
 * @return number sent ≥0, or -1 on EAGAIN or other error (errno set).
 */
ssize_t sock_send(const po_conn_t *c, const void *buf, size_t len) __nonnull((1));

/**
 * @brief Receive up to `len` bytes on this connection (nonblocking).
 * 
 * @return number received ≥0, 0 on orderly shutdown, or -1 on EAGAIN/ error.
 */
ssize_t sock_recv(const po_conn_t *c, void *buf, size_t len) __nonnull((1));

/**
 * @brief Access the internal poller to drive the event loop.
 * 
 * After calling `poller_wait()` you may call `sock_next_event()` to
 * retrieve ready‐fd events.
 */
poller_t *sock_poller(void);

/**
 * @brief Pop the next ready `poller_event_t` from the shared queue.
 * 
 * @return 1 if an event is returned in *ev, 0 if none left, -1 on error.
 */
int sock_next_event(poller_event_t **ev) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // _NET_SOCKET_H
