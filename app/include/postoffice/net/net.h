/**
 * @file net.h
 * @brief Networking module public API.
 * @details Combines the socket, framing, and protocol submodules to provide
 * a simple message-based networking interface. It uses efficient zero-copy
 * buffers and batched I/O for high-performance message exchange.
 */
#ifndef _NET_H
#define _NET_H

#include <stdint.h>
#include <sys/cdefs.h>
#include "net/protocol.h"
#include "net/framing.h"
#include "net/socket.h"


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
int net_send_message(int fd, uint8_t msg_type, uint8_t flags,
                     const uint8_t *payload, uint32_t payload_len) __nonnull((4));

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
int net_recv_message(int fd, po_header_t *header_out, zcp_buffer_t **payload_out)
    __nonnull((2,3));

#endif // _NET_H
