/**
 * @file protocol.h
 * @brief Protocol header definition and utilities.
 *
 * The protocol header is intentionally compact and packed for wire
 * compatibility. Fields whose natural representation differs between host
 * endiannesses are carried in network byte order over the wire. The API
 * provides conversion helpers and safe access patterns.
 *
 * Wire layout (packed, network order):
 * @verbatim
 * +----------------+----------+---------+----------------+
 * |  uint16_t ver  | uint8_t  | uint8_t |   uint32_t     |
 * |  (network)     | msg_type | flags   |   payload_len  |
 * +----------------+----------+---------+----------------+
 *      2 bytes        1 byte   1 byte    4 bytes
 * @endverbatim
 *
 * Design rationale and links:
 * - Fixed-length compact header keeps parsing cheap and deterministic.
 * - Version field allows forward/backward compatibility.
 * - Flags are a bitmask for optional per-message semantics (compression,
 *   encryption, priority, etc.).
 *
 * See also: RFC 791 (IPv4) for endianness conventions and POSIX socket
 * documentation for read/write semantics.
 */

#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <stdint.h>
#include <arpa/inet.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fixed protocol version (host order numeric constant).
 *
 * This constant is the logical version. On-the-wire the field is stored
 * in network byte order (htons/ntohs conversions are performed).
 */
#define PROTOCOL_VERSION 0x0001u

#pragma pack(push,1)
/**
 * @brief On-the-wire message header (packed, network byte order fields).
 *
 * All fields are packed to avoid unwanted padding bytes. The `payload_len`
 * counts the exact number of payload bytes that follow the header on the
 * wire (not including the 4-byte framing length prefix used by framing.h).
 */
typedef struct {
    uint16_t version;      /**< Protocol version (network byte order) */
    uint8_t  msg_type;     /**< Message type identifier */
    uint8_t  flags;        /**< Message flags (bitmask) */
    uint32_t payload_len;  /**< Length of the payload in bytes (network order) */
} po_header_t;
#pragma pack(pop)

/**
 * @name Flag bits for po_header_t::flags
 * @{ */
enum {
    PO_FLAG_NONE       = 0x00u, /**< No special flags */
    PO_FLAG_COMPRESSED = 0x01u, /**< Payload is compressed */
    PO_FLAG_ENCRYPTED  = 0x02u, /**< Payload is encrypted */
    PO_FLAG_URGENT     = 0x04u, /**< High priority / expedited processing */
};
/** @} */

/**
 * @brief Initialize a protocol header.
 *
 * This helper fills @p header with the canonical wire representation for the
 * given fields. The produced header is in network byte order (suitable for
 * sending directly with writev after prepending the 4-byte framing length
 * prefix). Use protocol_header_to_host() to convert a received header into
 * host byte order for inspection.
 *
 * @param header Pointer to the po_header_t to initialize (non-NULL).
 * @param msg_type Message type identifier.
 * @param flags Message flags bitmask.
 * @param payload_len Payload length in host byte order.
 */
static inline void protocol_init_header(po_header_t *header, uint8_t msg_type,
                                        uint8_t flags, uint32_t payload_len)
                                        __attribute__((nonnull(1)));
static inline void protocol_init_header(po_header_t *header, uint8_t msg_type,
                                        uint8_t flags, uint32_t payload_len)
{
    header->version = htons((uint16_t)PROTOCOL_VERSION);
    header->msg_type = msg_type;
    header->flags = flags;
    header->payload_len = htonl(payload_len);
}

/**
 * @brief Convert header fields from host to network byte order.
 *
 * Use this if you built a header in host byte order and wish to serialize it.
 * @param header Pointer to the po_header_t to convert (non-NULL).
 */
void protocol_header_to_network(po_header_t *header) __attribute__((nonnull(1)));

/**
 * @brief Convert header fields from network to host byte order.
 *
 * Use this after reading a header from the network to obtain host-order
 * integers for use in application logic.
 * @param header Pointer to the po_header_t to convert (non-NULL).
 */
void protocol_header_to_host(po_header_t *header) __attribute__((nonnull(1)));

/**
 * @brief Compute the total message size (header + payload) in host order.
 *
 * The function assumes @p header is already in host byte order (call
 * protocol_header_to_host() if needed). The returned size does not include
 * the 4-byte framing length prefix used by the framing layer.
 *
 * @param header Pointer to a po_header_t (host order) (non-NULL).
 * @return Total bytes comprising header + payload.
 */
uint32_t protocol_message_size(const po_header_t *header) __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif /* _PROTOCOL_H */
