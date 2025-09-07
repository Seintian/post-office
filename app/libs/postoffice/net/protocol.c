/**
 * @file protocol.c
 * @brief Protocol header utilities implementation.
 */

#include "net/protocol.h"
#include "metrics/metrics.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

int protocol_encode(
    uint8_t msg_type,
    uint8_t flags,
    const void *payload,
    uint32_t payload_len,
    po_header_t *out_hdr
) {
    if (!out_hdr)
        return -1;

    if (payload_len && !payload) {
        PO_METRIC_COUNTER_INC("protocol.encode.invalid");
        return -1;
    }

    protocol_init_header(out_hdr, msg_type, flags, payload_len);

    PO_METRIC_COUNTER_INC("protocol.encode.ok");
    PO_METRIC_COUNTER_ADD("protocol.encode.bytes", payload_len);
    return 0;
}

int protocol_decode(
    const po_header_t *net_hdr,
    void *payload_buf,
    uint32_t payload_buf_size,
    uint32_t *payload_len_out
) {
    if (!net_hdr)
        return -1;

    po_header_t tmp = *net_hdr; // copy to convert
    protocol_header_to_host(&tmp);

    uint32_t need = tmp.payload_len;
    if (need > 0) {
        if (!payload_buf || payload_buf_size < need) {
            PO_METRIC_COUNTER_INC("protocol.decode.emsgsize_buf");
            return -1;
        }
    }
    if (payload_len_out)
        *payload_len_out = need;

    PO_METRIC_COUNTER_INC("protocol.decode.ok");
    PO_METRIC_COUNTER_ADD("protocol.decode.bytes", need);
    return 0;
}

void protocol_header_to_network(po_header_t *header) {
    header->version = htons((uint16_t)PROTOCOL_VERSION);

    // msg_type and flags are single bytes, unchanged
    header->payload_len = htonl(header->payload_len);
}

void protocol_header_to_host(po_header_t *header) {
    header->version = ntohs(header->version);
    header->payload_len = ntohl(header->payload_len);
}

uint32_t protocol_message_size(const po_header_t *header) {
    // assume host order
    return (uint32_t)sizeof(po_header_t) + header->payload_len;
}
