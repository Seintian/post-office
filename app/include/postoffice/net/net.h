/**
 * @file net.h
 * @brief Networking module public API.
 * @details Combines the socket, framing, and protocol submodules-frame based
 * networking interface.
 *
 * NOTE: This header no longer includes the Poller API. Use <postoffice/net/poller.h>
 * for event loop functionality.
 *
 * @ingroup net
 */

#ifndef _NET_H
#define _NET_H

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/epoll.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fixed protocol version (host order numeric constant).
 */
#define PROTOCOL_VERSION 0x0001u

#pragma pack(push, 1)
/**
 * @brief On-the-wire message header (packed, network byte order fields).
 */
typedef struct {
    uint16_t version;     /**< Protocol version (network byte order) */
    uint8_t msg_type;     /**< Message type identifier */
    uint8_t flags;        /**< Message flags (bitmask) */
    uint32_t payload_len; /**< Length of the payload in bytes (network order) */
} po_header_t;
#pragma pack(pop)

/**
 * @name Flag bits for po_header_t::flags
 * @brief Bitmask namespace for per-message qualifiers.
 * @{ */
enum {
    PO_FLAG_NONE = 0x00u,       /**< No special flags */
    PO_FLAG_COMPRESSED = 0x01u, /**< Payload is compressed */
    PO_FLAG_ENCRYPTED = 0x02u,  /**< Payload is encrypted */
    PO_FLAG_URGENT = 0x04u,     /**< High priority / expedited processing */
};
/** @} */

/* Zero-copy buffer type alias (raw bytes) provided by perf/zero-copy */
typedef uint8_t zcp_buffer_t;

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
 * Initializes the memory pools required for message transmission and reception.
 * Must be called before using any receive functions.
 *
 * @param tx_buffers Number of TX buffers.
 * @param rx_buffers Number of RX buffers.
 * @param buf_size Size of each buffer (bytes).
 * @return 0 on success, -1 on failure.
 */
int net_init_zerocopy(size_t tx_buffers, size_t rx_buffers, size_t buf_size);

/**
 * @brief Shutdown and release process-wide zero-copy pools.
 * 
 * Safe to call even if not initialized.
 */
void net_shutdown_zerocopy(void);

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
 * @note This function requires `net_init_zerocopy` to have been called.
 * @note The caller is responsible for releasing the returned payload buffer
 * (via net_zcp_release_rx(*payload_out)) when finished with it.
 * @note For non-blocking sockets, this function is atomic: it will either read
 * the full message or return -1 (with errno=EAGAIN) without consuming partial data,
 * preventing stream corruption.
 *
 * @param fd The socket file descriptor to read from.
 * @param header_out Pointer to a po_header_t to fill with the received header.
 * @param payload_out Pointer to a zcp_buffer_t* that will be set to the payload buffer.
 * @return 0 on success, or a negative error code on failure.
 */
int net_recv_message(int fd, po_header_t *header_out, zcp_buffer_t **payload_out) __nonnull((2, 3));

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _NET_H
