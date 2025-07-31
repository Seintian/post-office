#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "net/protocol.h"
#include "utils/errors.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>


#define TOTAL_LEN_SIZE 4
#define PROTO_HEADER_SIZE (sizeof(po_header_t))

int po_proto_encode(
    framing_encoder_t       *enc,
    uint8_t                  msg_type,
    uint8_t                  flags,
    const void              *payload,
    uint32_t                 payload_len,
    void                   **out_frame,
    uint32_t                *out_frame_len
) {
    if (payload_len > UINT32_MAX - PROTO_HEADER_SIZE) {
        errno = NET_EINVAL;
        return -1;
    }

    // total payload for framing = protocol header + application payload
    uint32_t proto_payload = PROTO_HEADER_SIZE + payload_len;

    // Acquire one buffer from the pool
    void *buf = perf_zcpool_acquire(enc->pool);
    if (!buf) {
        // errno set by zcpool (EAGAIN or ENOMEM)
        return -1;
    }

    // We need to write:
    // [0..3]   4‑byte BE total_frame_len (header+payload)
    // [4..]    po_header_t (network order)
    // [...]    user payload

    uint8_t *p = buf;

    // 1) length prefix = total_frame_len = PROTO_HEADER_SIZE + payload_len
    uint32_t total_be = htonl(proto_payload);
    memcpy(p, &total_be, TOTAL_LEN_SIZE);
    p += TOTAL_LEN_SIZE;

    // 2) protocol header in‑place
    po_header_t hdr;
    hdr.version     = htons(PO_PROTO_VERSION);
    hdr.msg_type    = msg_type;
    hdr.flags       = flags;
    hdr.payload_len = htonl(payload_len);
    memcpy(p, &hdr, PROTO_HEADER_SIZE);
    p += PROTO_HEADER_SIZE;

    // 3) payload
    if (payload_len && payload)
        memcpy(p, payload, payload_len);

    *out_frame     = buf;
    *out_frame_len = TOTAL_LEN_SIZE + proto_payload;
    return 0;
}

int po_proto_decode(
    framing_decoder_t *dec,
    uint8_t           *out_msg_type,
    uint8_t           *out_flags,
    void             **out_payload,
    uint32_t          *out_payload_len
) {
    // Pull next frame off the decoder's ring.
    // framing_decoder_next returns:
    //   1 if a frame was dequeued, and *fp points to the start of the user‑supplied buffer
    //          i.e. immediately after the 4B framing header.
    //   0 if none ready
    //  -1 on error
    void *frame_buf;
    uint32_t frame_payload_len;
    int rc = framing_decoder_next(dec, &frame_buf, &frame_payload_len);
    if (rc <= 0) {
        // empty (0) or error (-1 with errno from framing)
        return rc;
    }

    // Now frame_buf points to the first byte after our 4B framing header,
    // which is the start of a packed po_header_t:
    if (frame_payload_len < PROTO_HEADER_SIZE) {
        // malformed: not even enough for our header
        errno = NET_EPROTO;
        // release buffer back to pool before returning
        perf_zcpool_release(dec->pool, frame_buf);
        return -1;
    }

    uint8_t *p = frame_buf;
    po_header_t hdr_be;
    memcpy(&hdr_be, p, PROTO_HEADER_SIZE);

    // convert to host order
    uint16_t version     = ntohs(hdr_be.version);
    uint32_t payload_len = ntohl(hdr_be.payload_len);

    // check version
    if (version != PO_PROTO_VERSION) {
        errno = NET_EPROTONOSUPPORT;
        perf_zcpool_release(dec->pool, frame_buf);
        return -1;
    }

    *out_msg_type    = hdr_be.msg_type;
    *out_flags       = hdr_be.flags;
    *out_payload_len = payload_len;
    // payload starts after the header in place
    *out_payload     = p + PROTO_HEADER_SIZE;

    // Caller now owns frame_buf (they must call perf_zcpool_release(pool, frame_buf))
    return 1;
}
