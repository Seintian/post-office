/**
 * @file protocol.h
 * @brief Protocol header definition and utilities.
 * @ingroup net
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
 * - Separation of framing length prefix (see framing.h) from protocol header
 *   isolates transport-level reassembly from application semantics.
 *
 * See also: RFC 791 (IPv4) for endianness conventions and POSIX socket
 * documentation for read/write semantics.
 */

#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <arpa/inet.h>
#include <stdint.h>
#include <sys/cdefs.h>

#include "net/net.h"

#ifdef __cplusplus
extern "C" {
#endif

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
static inline void protocol_init_header(po_header_t *header, uint8_t msg_type, uint8_t flags,
                                        uint32_t payload_len) __attribute__((nonnull(1)));
static inline void protocol_init_header(po_header_t *header, uint8_t msg_type, uint8_t flags,
                                        uint32_t payload_len) {
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

/**
 * @brief Encode a protocol header for a payload.
 *
 * Invariants enforced:
 *  - payload_len may be zero (valid empty payload).
 *  - version is always set to PROTOCOL_VERSION.
 *  - Flags are not validated here; higher layers ensure semantic consistency.
 *
 * Emits metrics (component.operation.metric):
 *  - protocol.encode.ok (success path)
 *  - protocol.encode.invalid (invalid arguments)
 *  - protocol.encode.bytes (payload length accumulation)
 *
 * @param msg_type Message type identifier.
 * @param flags Message flags bitmask.
 * @param payload Pointer to payload (may be NULL if payload_len == 0).
 * @param payload_len Payload length (host order).
 * @param out_hdr Destination header pointer (host order fields on return).
 * @return 0 on success, -1 on error.
 */
int protocol_encode(uint8_t msg_type, uint8_t flags, const void *payload, uint32_t payload_len,
                    po_header_t *out_hdr);

/**
 * @brief Validate a received header (network order) and report payload length.
 *
 * Validation includes version matching and payload length sanity checks
 * (caller may further enforce maximums alongside framing module limits).
 *
 * The function copies/converts the header internally for validation. It
 * does not copy payload bytes; callers should subsequently read/consume
 * the payload based on the returned length.
 *
 * Metrics emitted:
 *  - protocol.decode.ok
 *  - protocol.decode.emsgsize_buf (provided buffer too small)
 *  - protocol.decode.bytes (payload length accumulation)
 *
 * @param net_hdr Pointer to header as received from network (network order payload_len).
 * @param payload_buf Optional buffer pointer (NULL allowed if expecting length only).
 * @param payload_buf_size Size of provided buffer.
 * @param payload_len_out Optional out parameter for payload length (host order).
 * @return 0 on success, -1 on error.
 */
int protocol_decode(const po_header_t *net_hdr, void *payload_buf, uint32_t payload_buf_size,
                    uint32_t *payload_len_out) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif /* _PROTOCOL_H */
