#ifndef _NET_PROTOCOL_H
#define _NET_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "perf/zerocopy.h"
#include "net/framing.h"


#pragma pack(push,1)
typedef struct {
    uint16_t version;     // must be PO_PROTO_VERSION in network order
    uint8_t  msg_type;
    uint8_t  flags;
    uint32_t payload_len; // network order
} po_header_t;
#pragma pack(pop)

#define PO_PROTO_VERSION 0x0001
#define PO_HEADER_SIZE   sizeof(po_header_t)

/** Build one wire‐message in a zcpool buffer: [4B frame‑len][po_header][payload]. */
int po_proto_encode(
    framing_encoder_t       *enc,
    uint8_t                  msg_type,
    uint8_t                  flags,
    const void              *payload,
    uint32_t                 payload_len,
    void                   **out_frame,
    uint32_t                *out_frame_len
) __nonnull((1, 6, 7));

/** 
 * Peel off one framed buffer from ring + decoding, verify and parse its header.
 * Returns 1 on success (you get a pointer to payload region),
 *         0 if none ready,
 *        -1 on malformed/version mismatch.
 */
int po_proto_decode(
    framing_decoder_t *dec,
    uint8_t           *out_msg_type,
    uint8_t           *out_flags,
    void             **out_payload,
    uint32_t          *out_payload_len
) __nonnull((1, 2, 3, 4, 5));

#endif // _NET_PROTOCOL_H
