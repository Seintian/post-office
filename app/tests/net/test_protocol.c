// tests/net/test_protocol.c
#include "unity/unity_fixture.h"
#include "net/protocol.h"
#include <string.h>

TEST_GROUP(PROTOCOL);

TEST_SETUP(PROTOCOL) { /* no-op */ }
TEST_TEAR_DOWN(PROTOCOL) { /* no-op */ }

TEST(PROTOCOL, InitAndByteOrderRoundtrip) {
	po_header_t h = {0};
	// Build in host order first
	h.version = (uint16_t)PROTOCOL_VERSION;
	h.msg_type = 0x42u;
	h.flags = (uint8_t)(PO_FLAG_COMPRESSED | PO_FLAG_URGENT);
	h.payload_len = 1234u;

	// Convert to network and back
	protocol_header_to_network(&h);
	// After to_network, msg_type and flags stay unchanged
	TEST_ASSERT_EQUAL_HEX8(0x42u, h.msg_type);
	TEST_ASSERT_EQUAL_HEX8((PO_FLAG_COMPRESSED | PO_FLAG_URGENT), h.flags);

	protocol_header_to_host(&h);
	TEST_ASSERT_EQUAL_HEX16(PROTOCOL_VERSION, h.version);
	TEST_ASSERT_EQUAL_HEX8(0x42u, h.msg_type);
	TEST_ASSERT_EQUAL_HEX8((PO_FLAG_COMPRESSED | PO_FLAG_URGENT), h.flags);
	TEST_ASSERT_EQUAL_UINT32(1234u, h.payload_len);

	// Helper init produces network order directly
	po_header_t h2;
	protocol_init_header(&h2, 0x01u, PO_FLAG_NONE, 0u);
	// After converting to host order: values must match inputs
	protocol_header_to_host(&h2);
	TEST_ASSERT_EQUAL_HEX16(PROTOCOL_VERSION, h2.version);
	TEST_ASSERT_EQUAL_HEX8(0x01u, h2.msg_type);
	TEST_ASSERT_EQUAL_HEX8(PO_FLAG_NONE, h2.flags);
	TEST_ASSERT_EQUAL_UINT32(0u, h2.payload_len);
}

TEST_GROUP_RUNNER(PROTOCOL) {
	RUN_TEST_CASE(PROTOCOL, InitAndByteOrderRoundtrip);
}

TEST(PROTOCOL, MessageSizeComputation) {
	po_header_t h_host = {0};
	h_host.version = PROTOCOL_VERSION;
	h_host.msg_type = 0xAAu;
	h_host.flags = PO_FLAG_ENCRYPTED;
	h_host.payload_len = 4096u;
	uint32_t total = protocol_message_size(&h_host);
	TEST_ASSERT_EQUAL_UINT32(sizeof(po_header_t) + 4096u, total);
}

TEST(PROTOCOL, LargePayloadBoundary) {
	// Ensure 32-bit payload length conversion survives high values
	po_header_t h;
	const uint32_t len = 64u * 1024u * 1024u; // 64 MiB boundary used elsewhere
	protocol_init_header(&h, 0x7Fu, PO_FLAG_COMPRESSED, len);
	// After conversion back to host, value must match
	protocol_header_to_host(&h);
	TEST_ASSERT_EQUAL_UINT32(len, h.payload_len);
	TEST_ASSERT_EQUAL_HEX8(0x7Fu, h.msg_type);
}

TEST_GROUP_RUNNER(PROTOCOL_EXT) {
	RUN_TEST_CASE(PROTOCOL, MessageSizeComputation);
	RUN_TEST_CASE(PROTOCOL, LargePayloadBoundary);
}



