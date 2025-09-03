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
 */

#ifndef _FRAMING_H
#define _FRAMING_H

#include <stdint.h>
#include <sys/cdefs.h>
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of zero-copy buffer type provided by perf/zero-copy */
typedef struct zcp_buffer zcp_buffer_t;

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
 */
uint32_t framing_get_max_payload(void);

/**
 * @brief Read a full message from a socket into a zero-copy buffer.
 *
 * This function implements a blocking/read-until-complete semantic on the
 * provided file descriptor. It will allocate a zcp_buffer_t for the payload
 * (from the perf zero-copy pool) and fill the header_out with the header in
 * host byte order.
 *
 * Ownership:
 * - On success: *payload_out is a reference to a zcp_buffer_t. The caller is
 *   responsible for calling zcp_release(*payload_out) when done.
 * - On failure: *payload_out is left unchanged.
 *
 * Return values:
 * - 0 on success
 * - -1 on error (errno set)
 * - -2 if peer closed the connection (EOF)
 *
 * Note: This function uses blocking I/O semantics by repeatedly calling read
 * until the full message is received. For integration with an event-driven
 * poller, use a non-blocking, incremental decoder variant (not exported
 * here) implemented internally in the framing module.
 */
int framing_read_msg(int fd, po_header_t *header_out, zcp_buffer_t **payload_out)
    __nonnull((2,3));

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
int framing_write_msg(int fd, const po_header_t *header, const uint8_t *payload, uint32_t payload_len)
    __nonnull((2));

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
    __nonnull((2,3));

#ifdef __cplusplus
}
#endif

#endif /* _FRAMING_H */
