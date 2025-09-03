// tests/net/test_net.c
#include "net/net.h"
#include "unity/unity_fixture.h"
#include <string.h>
#include <sys/socket.h>

TEST_GROUP(NET);

TEST_SETUP(NET) { framing_init(0); }
TEST_TEAR_DOWN(NET) { /* nothing to cleanup */ }

TEST(NET, SENDRECVEMPTYPAYLOAD) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    uint8_t dummy = 0; // pass non-null pointer to satisfy nonnull contract when len==0
    int rc = net_send_message(sv[0], 0x33u, PO_FLAG_NONE, &dummy, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);
    po_header_t hdr;
    zcp_buffer_t *buf = NULL;
    rc = net_recv_message(sv[1], &hdr, &buf);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NULL(buf);
    TEST_ASSERT_EQUAL_HEX8(0x33u, hdr.msg_type);
    TEST_ASSERT_EQUAL_UINT32(0u, hdr.payload_len);
    socket_close(sv[0]);
    socket_close(sv[1]);
}

TEST(NET, SENDRECVSMALLPAYLOAD) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    const char payload[] = "abc";
    int rc = net_send_message(sv[0], 0x34u, PO_FLAG_NONE, (const uint8_t *)payload,
                              (uint32_t)sizeof payload);
    TEST_ASSERT_EQUAL_INT(0, rc);
    po_header_t hdr;
    zcp_buffer_t *buf = NULL;
    rc = net_recv_message(sv[1], &hdr, &buf);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NULL(buf); // framing discards payload until zero-copy wired
    TEST_ASSERT_EQUAL_HEX8(0x34u, hdr.msg_type);
    TEST_ASSERT_EQUAL_UINT32(sizeof payload, hdr.payload_len);
    socket_close(sv[0]);
    socket_close(sv[1]);
}

TEST_GROUP_RUNNER(NET) {
    RUN_TEST_CASE(NET, SENDRECVEMPTYPAYLOAD);
    RUN_TEST_CASE(NET, SENDRECVSMALLPAYLOAD);
}

TEST(NET, SENDRECVBACKTOBACKMESSAGES) {
    framing_init(0);
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    const char p1[] = "one";
    const char p2[] = "two";
    TEST_ASSERT_EQUAL_INT(0, net_send_message(sv[0], 0x41u, PO_FLAG_URGENT, (const uint8_t *)p1,
                                              (uint32_t)sizeof p1));
    TEST_ASSERT_EQUAL_INT(0, net_send_message(sv[0], 0x42u, PO_FLAG_COMPRESSED, (const uint8_t *)p2,
                                              (uint32_t)sizeof p2));

    po_header_t h;
    zcp_buffer_t *buf = NULL;
    TEST_ASSERT_EQUAL_INT(0, net_recv_message(sv[1], &h, &buf));
    TEST_ASSERT_EQUAL_HEX8(0x41u, h.msg_type);
    TEST_ASSERT_EQUAL_HEX8(PO_FLAG_URGENT, h.flags);
    TEST_ASSERT_EQUAL_UINT32(sizeof p1, h.payload_len);
    TEST_ASSERT_NULL(buf);

    TEST_ASSERT_EQUAL_INT(0, net_recv_message(sv[1], &h, &buf));
    TEST_ASSERT_EQUAL_HEX8(0x42u, h.msg_type);
    TEST_ASSERT_EQUAL_HEX8(PO_FLAG_COMPRESSED, h.flags);
    TEST_ASSERT_EQUAL_UINT32(sizeof p2, h.payload_len);
    TEST_ASSERT_NULL(buf);

    socket_close(sv[0]);
    socket_close(sv[1]);
}

TEST(NET, LARGE_PAYLOADBOUNDARYHEADERONLY) {
    // We cannot actually allocate/send 64 MiB in tests; check header path only
    const uint32_t len = 64u * 1024u * 1024u; // allowed cap
    po_header_t h;
    protocol_init_header(&h, 0x55u, PO_FLAG_ENCRYPTED, len);
    protocol_header_to_host(&h);
    TEST_ASSERT_EQUAL_UINT32(len, h.payload_len);
}

TEST_GROUP_RUNNER(NET_EXT) {
    RUN_TEST_CASE(NET, SENDRECVBACKTOBACKMESSAGES);
    RUN_TEST_CASE(NET, LARGE_PAYLOADBOUNDARYHEADERONLY);
}
