// tests/net/test_framing.c
#include "net/framing.h"
#include "net/protocol.h"
#include "unity/unity_fixture.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

TEST_GROUP(FRAMING);

TEST_SETUP(FRAMING) {
    // Use default max payload
    framing_init(0);
}

TEST_TEAR_DOWN(FRAMING) { /* nothing to cleanup */ }

TEST(FRAMING, ROUNDTRIPEMPTYPAYLOAD) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    po_header_t h; // construct network-order header for empty payload
    protocol_init_header(&h, 0x10u, PO_FLAG_NONE, 0u);
    int rc = framing_write_msg(sv[0], &h, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);

    po_header_t out;
    zcp_buffer_t *payload = NULL;
    rc = framing_read_msg(sv[1], &out, &payload);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NULL(payload);
    TEST_ASSERT_EQUAL_HEX16(PROTOCOL_VERSION, out.version);
    TEST_ASSERT_EQUAL_HEX8(0x10u, out.msg_type);
    TEST_ASSERT_EQUAL_HEX8(PO_FLAG_NONE, out.flags);
    TEST_ASSERT_EQUAL_UINT32(0u, out.payload_len);
    close(sv[0]);
    close(sv[1]);
}

TEST(FRAMING, ROUNDTRIPSMALLPAYLOAD) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    const char msg[] = "hi";
    po_header_t h;
    protocol_init_header(&h, 0x20u, PO_FLAG_NONE, (uint32_t)sizeof msg);
    int rc = framing_write_msg(sv[0], &h, (const uint8_t *)msg, (uint32_t)sizeof msg);
    TEST_ASSERT_EQUAL_INT(0, rc);

    po_header_t out;
    zcp_buffer_t *payload = NULL;
    rc = framing_read_msg(sv[1], &out, &payload);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NULL(payload); // zero-copy not wired yet, framing discards payload
    TEST_ASSERT_EQUAL_HEX16(PROTOCOL_VERSION, out.version);
    TEST_ASSERT_EQUAL_HEX8(0x20u, out.msg_type);
    TEST_ASSERT_EQUAL_UINT32(sizeof msg, out.payload_len);
    close(sv[0]);
    close(sv[1]);
}

TEST_GROUP_RUNNER(FRAMING) {
    RUN_TEST_CASE(FRAMING, ROUNDTRIPEMPTYPAYLOAD);
    RUN_TEST_CASE(FRAMING, ROUNDTRIPSMALLPAYLOAD);
}

// Additional tests

TEST(FRAMING, INITANDGETMAXPAYLOAD) {
    // default
    TEST_ASSERT_EQUAL_UINT(FRAMING_DEFAULT_MAX_PAYLOAD, framing_get_max_payload());
    // set custom smaller
    TEST_ASSERT_EQUAL_INT(0, framing_init(4096));
    TEST_ASSERT_EQUAL_UINT(4096u, framing_get_max_payload());
    // reject larger than cap (64 MiB)
    TEST_ASSERT_EQUAL_INT(-1, framing_init(65u * 1024u * 1024u));
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
    // restore default
    TEST_ASSERT_EQUAL_INT(0, framing_init(0));
}

TEST(FRAMING, READREJECTSTOTALSMALLERTHANHEADER) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    // Write a length prefix that is smaller than po_header_t
    uint32_t total = (uint32_t)sizeof(po_header_t) - 1u;
    uint32_t total_be = htonl(total);
    ssize_t wn = write(sv[0], &total_be, sizeof(total_be));
    TEST_ASSERT_EQUAL_INT(sizeof(total_be), (int)wn);
    po_header_t out;
    zcp_buffer_t *payload = NULL;
    int rc = framing_read_msg(sv[1], &out, &payload);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(EPROTO, errno);
    close(sv[0]);
    close(sv[1]);
}

TEST(FRAMING, READREJECTSBADVERSION) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    // Craft a header with wrong version
    po_header_t bad_hdr = {0};
    bad_hdr.version = htons((uint16_t)(PROTOCOL_VERSION + 1u));
    bad_hdr.msg_type = 0x01u;
    bad_hdr.flags = PO_FLAG_NONE;
    bad_hdr.payload_len = htonl(0u);
    uint32_t total = (uint32_t)sizeof(po_header_t);
    uint32_t total_be = htonl(total);
    (void)!write(sv[0], &total_be, sizeof total_be);
    (void)!write(sv[0], &bad_hdr, sizeof bad_hdr);
    po_header_t out;
    zcp_buffer_t *payload = NULL;
    int rc = framing_read_msg(sv[1], &out, &payload);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(EPROTONOSUPPORT, errno);
    close(sv[0]);
    close(sv[1]);
}

TEST(FRAMING, READREJECTSTOOLARGEPAYLOAD) {
    // Configure small limit
    TEST_ASSERT_EQUAL_INT(0, framing_init(8));
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    // Header declares payload 9 (>8)
    po_header_t h;
    protocol_init_header(&h, 0x02u, PO_FLAG_NONE, 9u);
    uint32_t total = (uint32_t)sizeof(po_header_t) + 9u;
    uint32_t total_be = htonl(total);
    (void)!write(sv[0], &total_be, sizeof total_be);
    (void)!write(sv[0], &h, sizeof h);
    po_header_t out;
    zcp_buffer_t *payload = NULL;
    int rc = framing_read_msg(sv[1], &out, &payload);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(EMSGSIZE, errno);
    close(sv[0]);
    close(sv[1]);
    // restore default
    TEST_ASSERT_EQUAL_INT(0, framing_init(0));
}

TEST(FRAMING, WRITEREJECTSTOOLARGEPAYLOAD) {
    // Set small max
    TEST_ASSERT_EQUAL_INT(0, framing_init(4));
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    po_header_t h;
    protocol_init_header(&h, 0x03u, PO_FLAG_NONE, 5u);
    uint8_t payload[5] = {0, 1, 2, 3, 4};
    int rc = framing_write_msg(sv[0], &h, payload, 5u);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(EMSGSIZE, errno);
    close(sv[0]);
    close(sv[1]);
    TEST_ASSERT_EQUAL_INT(0, framing_init(0));
}

TEST(FRAMING, WRITEZEROCOPYTREATEDASZEROPAYLOAD) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    po_header_t h;
    protocol_init_header(&h, 0x04u, PO_FLAG_NONE, 0u);
    // dummy opaque buffer
    struct zcp_buffer {
        int dummy;
    };
    struct zcp_buffer dummy = (struct zcp_buffer){0};
    int rc = framing_write_zcp(sv[0], &h, (const zcp_buffer_t *)&dummy);
    TEST_ASSERT_EQUAL_INT(0, rc);
    po_header_t out;
    zcp_buffer_t *payload = NULL;
    rc = framing_read_msg(sv[1], &out, &payload);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT32(0u, out.payload_len);
    TEST_ASSERT_NULL(payload);
    close(sv[0]);
    close(sv[1]);
}

TEST_GROUP_RUNNER(FRAMING_EXT) {
    RUN_TEST_CASE(FRAMING, INITANDGETMAXPAYLOAD);
    RUN_TEST_CASE(FRAMING, READREJECTSTOTALSMALLERTHANHEADER);
    RUN_TEST_CASE(FRAMING, READREJECTSBADVERSION);
    RUN_TEST_CASE(FRAMING, READREJECTSTOOLARGEPAYLOAD);
    RUN_TEST_CASE(FRAMING, WRITEREJECTSTOOLARGEPAYLOAD);
    RUN_TEST_CASE(FRAMING, WRITEZEROCOPYTREATEDASZEROPAYLOAD);
}
