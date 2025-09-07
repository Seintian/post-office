/**
 * @file net.h
 * @brief Networking module public API.
 * @details Combines the socket, framing, and protocol submodules to provide
 * a simple message-based networking interface. It uses efficient zero-copy
 * buffers and batched I/O for high-performance message exchange.
 * @ingroup net
 */
// Added C linkage guards for C++ consumers.
#ifndef _NET_H
#define _NET_H

#include <stdint.h>
#include <sys/cdefs.h>

#include "net/framing.h"
#include "net/protocol.h"
#include "net/socket.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send a protocol message on a connected socket.
 *
 * This function constructs a protocol header (version, type, flags, payload length)
 * and sends the length-prefixed message (header + payload) over the socket.
 *
 * @param fd The socket file descriptor to send the message on.
 * @param msg_type The message type identifier.
 * @param flags The message flags bitmask.
 * @param payload Pointer to the payload data to send.
 * @param payload_len Length of the payload in bytes.
 * @return 0 on success, or a negative error code on failure.
 */
int net_send_message(int fd, uint8_t msg_type, uint8_t flags, const uint8_t *payload,
                     uint32_t payload_len) __nonnull((4));

/**
 * @brief Initialize process-wide zero-copy pools for TX/RX.
 *
 * Creates two independent pools sized for typical workloads. Callers may
 * omit this and still use the classic APIs; zero-copy functions will return
 * errors if pools are unavailable.
 */
int net_init_zerocopy(size_t tx_buffers, size_t rx_buffers, size_t buf_size);

/** Acquire/release TX buffers from the process-wide pool. */
void *net_zcp_acquire_tx(void);
void net_zcp_release_tx(void *buf);

/** Acquire/release RX buffers from the process-wide pool. */
void *net_zcp_acquire_rx(void);
void net_zcp_release_rx(void *buf);

/**
 * @brief Send a message using a zero-copy payload buffer.
 *
 * The header is constructed from inputs; payload_buf must point to a buffer
 * of at least payload_len bytes acquired from the TX pool.
 */
int net_send_message_zcp(int fd, uint8_t msg_type, uint8_t flags, void *payload_buf,
                         uint32_t payload_len);

/**
 * @brief Receive a message into a zero-copy buffer.
 *
 * On success, returns 0 and sets header_out (host order), *payload_out to the
 * buffer pointer, and *payload_len_out to the number of bytes. Caller owns the
 * buffer and must release it via net_zcp_release_rx().
 */
int net_recv_message_zcp(int fd, po_header_t *header_out, void **payload_out,
                         uint32_t *payload_len_out) __nonnull((2, 3, 4));

/**
 * @brief Receive the next protocol message from a socket.
 *
 * Reads a complete message (length prefix, header, payload) from the socket.
 * On success, @p header_out contains the message header (converted to host byte order),
 * and @p payload_out points to a zero-copy buffer containing the payload.
 *
 * @param fd The socket file descriptor to read from.
 * @param header_out Pointer to a po_header_t to fill with the received header.
 * @param payload_out Pointer to a zcp_buffer_t* that will be set to the payload buffer.
 * @return 0 on success, or a negative error code on failure.
 *
 * @note The caller is responsible for releasing the returned payload buffer
 * (via zcp_release(*payload_out)) when finished with it.
 */
int net_recv_message(int fd, po_header_t *header_out, zcp_buffer_t **payload_out) __nonnull((2, 3));

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _NET_H
