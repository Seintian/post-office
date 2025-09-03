/**
 * @file protocol.c
 * @brief Protocol header utilities implementation.
 */

#include "net/protocol.h"

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
