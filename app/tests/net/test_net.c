#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "net/framing.h"
#include "net/net.h"
#include "net/poller.h"
#include "net/protocol.h"
#include "net/socket.h"
#include "unity/unity_fixture.h"

TEST_GROUP(NET);

TEST_SETUP(NET) {
    net_shutdown_zerocopy();
    // Default init for most tests
    net_init_zerocopy(16, 16, 4096);
    framing_init(0);
}

TEST_TEAR_DOWN(NET) {
    net_shutdown_zerocopy();
}

// --- Basic Net Tests ---

TEST(NET, SEND_RECV_EMPTY_PAYLOAD) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    uint8_t dummy = 0; // pass non-null pointer to satisfy nonnull contract when len==0
    int rc = net_send_message(sv[0], 0x33u, PO_FLAG_NONE, &dummy, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);
    po_header_t hdr;
    zcp_buffer_t *buf = NULL;
    rc = net_recv_message(sv[1], &hdr, &buf);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_HEX8(0x33u, hdr.msg_type);
    TEST_ASSERT_EQUAL_UINT32(0u, hdr.payload_len);
    net_zcp_release_rx(buf);
    po_socket_close(sv[0]);
    po_socket_close(sv[1]);
}

TEST(NET, SEND_RECV_SMALL_PAYLOAD) {
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
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_HEX8(0x34u, hdr.msg_type);
    TEST_ASSERT_EQUAL_UINT32(sizeof payload, hdr.payload_len);
    // Verify content (zcp_buffer_t is uint8_t)
    TEST_ASSERT_EQUAL_MEMORY(payload, buf, sizeof payload);
    net_zcp_release_rx(buf);
    po_socket_close(sv[0]);
    po_socket_close(sv[1]);
}

TEST(NET, SEND_RECV_BACK_TO_BACK_MESSAGES) {
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
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_MEMORY(p1, buf, sizeof p1);
    net_zcp_release_rx(buf);

    buf = NULL;
    TEST_ASSERT_EQUAL_INT(0, net_recv_message(sv[1], &h, &buf));
    TEST_ASSERT_EQUAL_HEX8(0x42u, h.msg_type);
    TEST_ASSERT_EQUAL_HEX8(PO_FLAG_COMPRESSED, h.flags);
    TEST_ASSERT_EQUAL_UINT32(sizeof p2, h.payload_len);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_MEMORY(p2, buf, sizeof p2);
    net_zcp_release_rx(buf);

    po_socket_close(sv[0]);
    po_socket_close(sv[1]);
}

TEST(NET, LARGE_PAYLOAD_BOUNDARY_HEADER_ONLY) {
    // We cannot actually allocate/send 64 MiB in tests; check header path only
    const uint32_t len = 64u * 1024u * 1024u; // allowed cap
    po_header_t h;
    protocol_init_header(&h, 0x55u, PO_FLAG_ENCRYPTED, len);
    protocol_header_to_host(&h);
    TEST_ASSERT_EQUAL_UINT32(len, h.payload_len);
}

// --- Advanced Net Tests ---

TEST(NET, ATOMIC_READ_PARTIAL_HEADER) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    TEST_ASSERT_EQUAL_INT(0, po_socket_set_nonblocking(sv[1]));

    const char payload[] = "atomic";
    uint32_t payload_len = (uint32_t)sizeof(payload);
    
    // Construct full message manually
    po_header_t h;
    protocol_init_header(&h, 0xAAu, PO_FLAG_NONE, payload_len);
    
    uint32_t total_len = sizeof(po_header_t) + payload_len;
    uint32_t len_be = htonl(total_len);
    
    // Buffer to hold entire wire message: [Len 4B][Header][Payload]
    uint8_t wire[4 + sizeof(po_header_t) + sizeof(payload)];
    memcpy(wire, &len_be, 4);
    memcpy(wire + 4, &h, sizeof(h));
    memcpy(wire + 4 + sizeof(h), payload, payload_len);
    
    // 1. Write 1 byte of Length
    TEST_ASSERT_EQUAL_INT(1, write(sv[0], wire, 1));
    
    po_header_t out_h;
    zcp_buffer_t *out_buf = NULL;
    int rc = net_recv_message(sv[1], &out_h, &out_buf);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(EAGAIN, errno);
    TEST_ASSERT_NULL(out_buf);

    // 2. Write rest of Length (3 bytes) + 1 byte of Header
    TEST_ASSERT_EQUAL_INT(4, write(sv[0], wire + 1, 4));
    rc = net_recv_message(sv[1], &out_h, &out_buf);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(EAGAIN, errno); // Still atomic wait
    
    // 3. Write rest of Header + part of Payload
    TEST_ASSERT_EQUAL_INT(sizeof(po_header_t) + 2, write(sv[0], wire + 5, sizeof(po_header_t) + 2));
    rc = net_recv_message(sv[1], &out_h, &out_buf);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(EAGAIN, errno);

    // 4. Write rest of Payload
    size_t written_so_far = 1 + 4 + sizeof(po_header_t) + 2;
    size_t remaining = sizeof(wire) - written_so_far;
    TEST_ASSERT_EQUAL_INT(remaining, write(sv[0], wire + written_so_far, remaining));
    
    // 5. Read Full Message
    rc = net_recv_message(sv[1], &out_h, &out_buf);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(out_buf);
    TEST_ASSERT_EQUAL_HEX8(0xAAu, out_h.msg_type);
    TEST_ASSERT_EQUAL_UINT32(payload_len, out_h.payload_len);
    TEST_ASSERT_EQUAL_MEMORY(payload, out_buf, payload_len);
    
    net_zcp_release_rx(out_buf);
    close(sv[0]);
    close(sv[1]);
}

TEST(NET, REJECT_HUGE_PAYLOAD) {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    
    // Construct header with 100MB payload
    po_header_t h;
    uint32_t big_len = 100 * 1024 * 1024;
    protocol_init_header(&h, 0xBB, 0, big_len);
    
    uint32_t total = sizeof(h) + big_len;
    uint32_t len_be = htonl(total);
    
    TEST_ASSERT_EQUAL_INT(4, write(sv[0], &len_be, 4));
    TEST_ASSERT_EQUAL_INT(sizeof(h), write(sv[0], &h, sizeof(h)));
    // Don't write payload, the header check should fail immediately
    
    po_header_t out_h;
    zcp_buffer_t *buf = NULL;
    int rc = net_recv_message(sv[1], &out_h, &buf);
    
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(EMSGSIZE, errno);
    
    close(sv[0]);
    close(sv[1]);
}

TEST(NET, REJECT_BAD_PROTOCOL_VERSION) {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    
    po_header_t h;
    protocol_init_header(&h, 0xCC, 0, 0);
    h.version = htons(0xFFFF); // Invalid version
    
    uint32_t total = sizeof(h); // payload 0
    uint32_t len_be = htonl(total);
    
    TEST_ASSERT_EQUAL_INT(4, write(sv[0], &len_be, 4));
    TEST_ASSERT_EQUAL_INT(sizeof(h), write(sv[0], &h, sizeof(h)));
    
    po_header_t out_h;
    zcp_buffer_t *buf = NULL;
    int rc = net_recv_message(sv[1], &out_h, &buf);
    
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(EPROTONOSUPPORT, errno);
    
    close(sv[0]);
    close(sv[1]);
}

// --- Poller Tests ---

TEST(NET, POLLER_CREATE_AND_DESTROY) {
    poller_t *p = poller_create();
    TEST_ASSERT_NOT_NULL(p);
    poller_destroy(p);
}

TEST(NET, POLLER_ADD_MODIFY_REMOVE_AND_WAIT) {
    poller_t *p = poller_create();
    TEST_ASSERT_NOT_NULL(p);

    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    // Initially, make fd[1] readable by writing one byte
    char ch = 'x';
    TEST_ASSERT_EQUAL_INT(1, (int)write(sv[0], &ch, 1));

    TEST_ASSERT_EQUAL_INT(0, poller_add(p, sv[1], EPOLLIN));

    struct epoll_event ev[4];
    int n = poller_wait(p, ev, 4, 1000);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_TRUE((ev[0].events & EPOLLIN) != 0);
    TEST_ASSERT_EQUAL_INT(sv[1], ev[0].data.fd);

    // Consume the byte, then modify interest to EPOLLOUT
    char r;
    TEST_ASSERT_EQUAL_INT(1, (int)read(sv[1], &r, 1));
    TEST_ASSERT_EQUAL_INT(0, poller_mod(p, sv[1], EPOLLOUT));

    // Socketpair endpoints are almost always writable
    n = poller_wait(p, ev, 4, 1000);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, n);

    // Remove and ensure it's gone
    TEST_ASSERT_EQUAL_INT(0, poller_remove(p, sv[1]));
    // Further modify should fail
    TEST_ASSERT_EQUAL_INT(-1, poller_mod(p, sv[1], EPOLLIN));

    close(sv[0]);
    close(sv[1]);
    poller_destroy(p);
}

TEST(NET, POLLER_WAIT_TIMEOUT) {
    poller_t *p = poller_create();
    TEST_ASSERT_NOT_NULL(p);
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    TEST_ASSERT_EQUAL_INT(0, poller_add(p, sv[1], EPOLLIN));
    // No data written, expect timeout
    struct epoll_event ev;
    int n = poller_wait(p, &ev, 1, 10);
    TEST_ASSERT_EQUAL_INT(0, n);
    close(sv[0]);
    close(sv[1]);
    poller_destroy(p);
}

TEST(NET, POLLER_WAKE_NO_EVENTS) {
    poller_t *p = poller_create();
    TEST_ASSERT_NOT_NULL(p);
    struct epoll_event ev[2];
    // Issue a wake and then wait with a long timeout; should return 0 (wake only) quickly.
    TEST_ASSERT_EQUAL_INT(0, poller_wake(p));
    int n = poller_wait(p, ev, 2, 1000);
    TEST_ASSERT_EQUAL_INT(0, n); // wake consumed, no external fds
    poller_destroy(p);
}

TEST(NET, POLLER_TIMED_WAIT_WAKE_BEFORE_TIMEOUT) {
    poller_t *p = poller_create();
    TEST_ASSERT_NOT_NULL(p);
    struct epoll_event ev[4];
    bool timed_out = false;
    // Trigger wake before calling timed wait to simulate asynchronous wake
    TEST_ASSERT_EQUAL_INT(0, poller_wake(p));
    int n = poller_timed_wait(p, ev, 4, 200, &timed_out);
    TEST_ASSERT_EQUAL_INT(0, n);  // wake only (no external events)
    TEST_ASSERT_FALSE(timed_out); // should not be reported as a timeout
    poller_destroy(p);
}

TEST_GROUP_RUNNER(NET) {
    // Basic
    RUN_TEST_CASE(NET, SEND_RECV_EMPTY_PAYLOAD);
    RUN_TEST_CASE(NET, SEND_RECV_SMALL_PAYLOAD);
    RUN_TEST_CASE(NET, SEND_RECV_BACK_TO_BACK_MESSAGES);
    RUN_TEST_CASE(NET, LARGE_PAYLOAD_BOUNDARY_HEADER_ONLY);
    
    // Advanced
    RUN_TEST_CASE(NET, ATOMIC_READ_PARTIAL_HEADER);
    RUN_TEST_CASE(NET, REJECT_HUGE_PAYLOAD);
    RUN_TEST_CASE(NET, REJECT_BAD_PROTOCOL_VERSION);
    
    // Poller
    RUN_TEST_CASE(NET, POLLER_CREATE_AND_DESTROY);
    RUN_TEST_CASE(NET, POLLER_ADD_MODIFY_REMOVE_AND_WAIT);
    RUN_TEST_CASE(NET, POLLER_WAIT_TIMEOUT);
    RUN_TEST_CASE(NET, POLLER_WAKE_NO_EVENTS);
    RUN_TEST_CASE(NET, POLLER_TIMED_WAIT_WAKE_BEFORE_TIMEOUT);
}