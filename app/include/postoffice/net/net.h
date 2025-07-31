#ifndef _NET_NET_H
#define _NET_NET_H

#include <stdint.h>
#include <stddef.h>
#include "net/socket.h"
#include "net/protocol.h"
#include "net/framing.h"
#include "perf/zerocopy.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the networking stack.
 *
 * Creates:
 *   - Global poller
 *   - Zero‑copy buffer pool for frames
 *   - Framing encoder
 *
 * @param max_events  Max simultaneous epoll events
 * @param buf_count   Number of buffers in the pool
 * @param buf_size    Size of each buffer
 * @param ring_depth  Depth of per‑connection frame queues
 * @return 0 on success, -1 on error (errno set)
 */
int po_init(
    size_t   max_events,
    size_t   buf_count,
    uint32_t buf_size,
    size_t   ring_depth
);

/** Shut down and free all global resources. */
void po_shutdown(void);

/** High‑level listen/connect/accept using sockets. */
po_conn_t *po_listen(const char *path, uint16_t port);
po_conn_t *po_connect(const char *path, uint16_t port);
int        po_accept(const po_conn_t *listener, po_conn_t **out);

/** Close a connection and free its decoder. */
void po_close(po_conn_t **conn);

/**
 * @brief Send a complete protocol message over a connection.
 *
 * Uses zero‑copy buffers and framing encoder internally.
 * Caller does NOT manage buffers.
 *
 * @return 0 on success, -1 on error
 */
int po_send_msg(
    po_conn_t   *conn,
    uint8_t      msg_type,
    uint8_t      flags,
    const void  *payload,
    uint32_t     payload_len
);

/**
 * @brief Receive next complete protocol message (non‑blocking).
 *
 * @return  1 if a message is ready,
 *          0 if none available,
 *         -1 on error (errno set).
 *
 * On success, *payload points inside a pool buffer. Caller must
 * eventually call perf_zcpool_release() on the frame buffer.
 */
int po_recv_msg(
    po_conn_t   *conn,
    uint8_t     *msg_type,
    uint8_t     *flags,
    void       **payload,
    uint32_t    *payload_len
);

#ifdef __cplusplus
}
#endif

#endif // _NET_NET_H
