/**
 * @file net.c
 * @brief High-level networking API built on protocol and framing.
 */

#include "net/net.h"
#include "net/protocol.h"
#include "net/framing.h"

int net_send_message(int fd, uint8_t msg_type, uint8_t flags,
					 const uint8_t *payload, uint32_t payload_len) {
	po_header_t hdr;
	protocol_init_header(&hdr, msg_type, flags, payload_len);
	// hdr is already network order
	return framing_write_msg(fd, &hdr, payload, payload_len);
}

int net_recv_message(int fd, po_header_t *header_out, zcp_buffer_t **payload_out) {
	// framing_read_msg already converts header to host order
	return framing_read_msg(fd, header_out, payload_out);
}

