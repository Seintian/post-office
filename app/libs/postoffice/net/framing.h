/**
 * @file framing.h
 * @brief Length-prefixed message framing with zero-copy support.
 * @ingroup net
 *
 * The framing layer implements the on-wire message format used by the
 * PostOffice stack. Messages on the wire are encoded as:
 *
 *   [4B length prefix][po_header_t][payload bytes]
 *
 * where the 4-byte length prefix (network order) contains the number of
 * bytes that follow the prefix (i.e. sizeof(po_header_t) + payload_len).
 *
 * The framing layer must handle partial reads/writes, arbitrary fragmentation
 * and coalescing performed by the kernel, and provide zero-copy semantics for
 * payload buffers via zcp_buffer_t (allocated from perf_zcpool_t).
 *
 * Error handling: Functions return 0 on success, -1 on error (with errno set),
 * and -2 for peer closure (EOF). EMSGSIZE is used when declared payload exceeds
 * configured maximum. EINVAL is used for malformed headers. ENOMEM may be
 * returned on allocation failure when acquiring zero-copy buffers.
 *
 * @see protocol.h
 * @see perf (zero-copy pool) subsystem for buffer ownership model.
 */

#ifndef _FRAMING_H
#define _FRAMING_H

#include <stdint.h>
#include <sys/cdefs.h>

#include "net/net.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default maximum frame payload size (in bytes).
 *
 * Default chosen to match the zero-copy pool backing big-pages: 2 MiB.
 * The framing implementation will refuse frames that declare a payload
 * larger than `framing_get_max_payload()`.
 */
#define FRAMING_DEFAULT_MAX_PAYLOAD (2u * 1024u * 1024u) /* 2 MiB */

/**
 * @brief Initialize the framing module.
 *
 * Must be called early during process initialization if the framed API is
 * used. Sets an optional maximum payload size (0 means use default).
 *
 * @param max_payload_bytes The maximum allowed payload size; if 0 the
 *        default (FRAMING_DEFAULT_MAX_PAYLOAD) is used.
 * @return 0 on success, -1 on error (errno set).
 */
int framing_init(uint32_t max_payload_bytes);

/**
 * @brief Get the configured maximum payload size in bytes.
 *
 * Useful for validating payload lengths before attempting sends.
 */
uint32_t framing_get_max_payload(void);

/**
 * @brief Write a message to a socket from a contiguous payload buffer.
 *
 * The header must already be in network byte order (use
 * protocol_header_to_network() or protocol_init_header()). The function will
 * handle partial writes internally and will return only after the entire
 * message (length prefix + header + payload) has been sent or an error
 * occurred.
 *
 * @param fd Socket file descriptor (non-blocking sockets are supported).
 * @param header Pointer to a po_header_t in network byte order (non-NULL).
 * @param payload Pointer to the contiguous payload bytes (non-NULL if payload_len>0).
 * @param payload_len Length of payload in bytes.
 * @return 0 on success, -1 on error (errno set), or -2 if the peer closed socket.
 */
int framing_write_msg(int fd, const po_header_t *header, const uint8_t *payload,
                      uint32_t payload_len) __nonnull((2));

/**
 * @brief Write a message to a socket using a zero-copy payload buffer.
 *
 * This variant avoids copying payload bytes into an intermediate buffer by
 * sending the protocol header and using the kernel-space buffer directly
 * where supported. Implementations may use writev/splice/sendfile depending
 * on the buffer backing mechanism. The header must be in network order.
 *
 * Ownership: The caller retains ownership of the zcp_buffer_t while this
 * function executes; the function will not take ownership.
 *
 * @param fd Socket file descriptor.
 * @param header Pointer to a po_header_t in network byte order (non-NULL).
 * @param payload A pointer to an allocated zcp_buffer_t containing the payload.
 * @return 0 on success, -1 on error (errno set), -2 if peer closed.
 */
int framing_write_zcp(int fd, const po_header_t *header, const zcp_buffer_t *payload)
    __nonnull((2, 3));

/**
 * @brief Read a message into a caller-provided buffer (no internal allocation).
 *
 * Use this variant when zero-copy buffers are unnecessary (small control
 * messages) or when the caller wants stack/arena ownership of payload bytes.
 *
 * If the payload length exceeds payload_buf_size, the function fails with EMSGSIZE.
 * On success, header_out is filled (host order) and *payload_len_out is set.
 */
int framing_read_msg_into(int fd, po_header_t *header_out, void *payload_buf,
                          uint32_t payload_buf_size, uint32_t *payload_len_out) __nonnull((2));

/**
 * @brief Read a message from a blocking socket (blocks until full message arrived).
 *
 * Use this variant for blocking clients (like the User process) that do not use
 * an event loop.
 *
 * @param fd Blocking socket file descriptor.
 * @param header_out Copied from framing_read_msg_into.
 * @param payload_buf Copied from framing_read_msg_into.
 * @param payload_buf_size Copied from framing_read_msg_into.
 * @param payload_len_out Copied from framing_read_msg_into.
 * @return 0 on success, -1 on error, -2 on EOF.
 */
int framing_read_msg_blocking(int fd, po_header_t *header_out, void *payload_buf,
                              uint32_t payload_buf_size, uint32_t *payload_len_out) __nonnull((2));

#ifdef __cplusplus
}
#endif

#endif /* _FRAMING_H */
