#ifndef _NET_NET_H
#define _NET_NET_H

#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _po_conn_t po_conn_t;

// Initialize global networking (epoll, thread‑pools, etc.)
int po_net_init(void);

// Start listening (Unix‑socket if port==0, else TCP)
po_conn_t *po_listen(const char *path, uint16_t port) __nonnull((1));

// Connect to a listener (Unix or TCP)
po_conn_t *po_connect(const char *path, uint16_t port) __nonnull((1));

/**
 * @brief Asynchronously send one application message.
 * 
 * Returns immediately after queueing; any write errors get surfaced
 * in a background thread (logged).
 */
int net_send(
    uint8_t     msg_type,
    uint8_t     flags,
    const void *payload,
    uint32_t    payload_len
) __nonnull((3));

/**
 * @brief Read from socket (blocking) and feed framing decoder.
 * 
 * Typically call once before net_recv() when you know data is available.
 */
ssize_t net_read_and_feed(void);

/**
 * @brief Try to pop the next complete message.
 * @param out_msg_type    Message type
 * @param out_flags       Flags
 * @param out_payload     Pointer to payload region (owned by pool)
 * @param out_payload_len Length of payload
 * @return  1 if you got a message,
 *          0 if none is ready,
 *         -1 on protocol error.
 */
int net_recv(
    uint8_t  *out_msg_type,
    uint8_t  *out_flags,
    void    **out_payload,
    uint32_t *out_payload_len
) __nonnull((1, 2, 3, 4));

/** When you're done with a `out_payload` from net_recv(), call this to recycle it. */
void net_payload_release(void *buf) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // _NET_NET_H
