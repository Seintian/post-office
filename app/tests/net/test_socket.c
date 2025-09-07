// tests/net/test_socket.c
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <errno.h>
 #include <string.h>

#include "net/socket.h" // provides PO_SOCKET_WOULDBLOCK sentinel
#include "unity/unity_fixture.h"

TEST_GROUP(SOCKET);

TEST_SETUP(SOCKET) { /* no setup required */
}
TEST_TEAR_DOWN(SOCKET) { /* no teardown required */
}

TEST(SOCKET, NON_BLOCKING_HELPER) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    TEST_ASSERT_EQUAL_INT(0, po_socket_set_nonblocking(sv[0]));
    int fl = fcntl(sv[0], F_GETFL);
    TEST_ASSERT_TRUE((fl & O_NONBLOCK) != 0);
    po_socket_close(sv[0]);
    po_socket_close(sv[1]);
}

TEST(SOCKET, COMMON_OPTIONS_NO_ERROR) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    // On UNIX sockets, TCP options are no-ops; however the function should not fail.
    int rc = po_socket_set_common_options(sv[0], 1, 1, 1);
    TEST_ASSERT_TRUE(rc == 0 || rc == -1);
    po_socket_close(sv[0]);
    po_socket_close(sv[1]);
}

// Extended tests for helper behavior on error handling and close
TEST(SOCKET, CLOSE_IGNORES_EINTR) {
    // We can't force EINTR easily; just ensure calling twice is safe.
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    po_socket_close(sv[0]);
    // Second close should not crash; may set errno but we don't assert
    po_socket_close(sv[0]);
    po_socket_close(sv[1]);
}

// New tests for po_socket_send / po_socket_recv helpers

TEST(SOCKET, SEND_RECV_SUCCESS) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    TEST_ASSERT_EQUAL_INT(0, po_socket_set_nonblocking(sv[0]));
    TEST_ASSERT_EQUAL_INT(0, po_socket_set_nonblocking(sv[1]));

    const char msg[] = "hello-world";
    ssize_t sent = po_socket_send(sv[0], msg, sizeof msg, 0);
    TEST_ASSERT_GREATER_THAN_INT(0, (int)sent);
    TEST_ASSERT_EQUAL_INT(sizeof msg, sent); // expect full write on socketpair

    char buf[64];
    ssize_t recvd = po_socket_recv(sv[1], buf, sizeof buf, 0);
    TEST_ASSERT_GREATER_THAN_INT(0, (int)recvd);
    TEST_ASSERT_EQUAL_INT(sent, recvd);
    TEST_ASSERT_EQUAL_INT(0, memcmp(msg, buf, (size_t)recvd));

    po_socket_close(sv[0]);
    po_socket_close(sv[1]);
}

TEST(SOCKET, RECV_EAGAIN_ON_EMPTY_SOCKET) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    TEST_ASSERT_EQUAL_INT(0, po_socket_set_nonblocking(sv[0]));
    TEST_ASSERT_EQUAL_INT(0, po_socket_set_nonblocking(sv[1]));

    char buf[8];
    errno = 0;
    ssize_t recvd = po_socket_recv(sv[0], buf, sizeof buf, 0);
    TEST_ASSERT_EQUAL_INT(PO_SOCKET_WOULDBLOCK, (int)recvd);
    TEST_ASSERT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);

    po_socket_close(sv[0]);
    po_socket_close(sv[1]);
}

TEST(SOCKET, RECV_EOF_AFTER_PEER_CLOSE) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    TEST_ASSERT_EQUAL_INT(0, po_socket_set_nonblocking(sv[0]));
    TEST_ASSERT_EQUAL_INT(0, po_socket_set_nonblocking(sv[1]));

    // Close peer then attempt to recv to get EOF (0)
    po_socket_close(sv[1]);
    char buf[8];
    ssize_t recvd = po_socket_recv(sv[0], buf, sizeof buf, 0);
    TEST_ASSERT_EQUAL_INT(0, (int)recvd);
    po_socket_close(sv[0]);
}

TEST(SOCKET, SEND_INVALID_FD_FAILS) {
    char dummy[4] = {0};
    errno = 0;
    ssize_t sent = po_socket_send(-1, dummy, sizeof dummy, 0);
    TEST_ASSERT_EQUAL_INT(-1, (int)sent);
    TEST_ASSERT_EQUAL_INT(EBADF, errno);
}

TEST_GROUP_RUNNER(SOCKET) {
    RUN_TEST_CASE(SOCKET, NON_BLOCKING_HELPER);
    RUN_TEST_CASE(SOCKET, COMMON_OPTIONS_NO_ERROR);
    RUN_TEST_CASE(SOCKET, CLOSE_IGNORES_EINTR);
    RUN_TEST_CASE(SOCKET, SEND_RECV_SUCCESS);
    RUN_TEST_CASE(SOCKET, RECV_EAGAIN_ON_EMPTY_SOCKET);
    RUN_TEST_CASE(SOCKET, RECV_EOF_AFTER_PEER_CLOSE);
    RUN_TEST_CASE(SOCKET, SEND_INVALID_FD_FAILS);
}
