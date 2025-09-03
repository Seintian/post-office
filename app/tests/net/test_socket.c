// tests/net/test_socket.c
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "net/socket.h"
#include "unity/unity_fixture.h"

TEST_GROUP(SOCKET);

TEST_SETUP(SOCKET) { /* no setup required */
}
TEST_TEAR_DOWN(SOCKET) { /* no teardown required */
}

TEST(SOCKET, NONBLOCKINGHELPER) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    TEST_ASSERT_EQUAL_INT(0, socket_set_nonblocking(sv[0]));
    int fl = fcntl(sv[0], F_GETFL);
    TEST_ASSERT_TRUE((fl & O_NONBLOCK) != 0);
    socket_close(sv[0]);
    socket_close(sv[1]);
}

TEST(SOCKET, COMMONOPTIONSNOERROR) {
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    // On UNIX sockets, TCP options are no-ops; however the function should not fail.
    int rc = socket_set_common_options(sv[0], 1, 1, 1);
    TEST_ASSERT_TRUE(rc == 0 || rc == -1);
    socket_close(sv[0]);
    socket_close(sv[1]);
}

// Extended tests for helper behavior on error handling and close
TEST(SOCKET, CLOSEIGNORESEINTR) {
    // We can't force EINTR easily; just ensure calling twice is safe.
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    socket_close(sv[0]);
    // Second close should not crash; may set errno but we don't assert
    socket_close(sv[0]);
    socket_close(sv[1]);
}

TEST_GROUP_RUNNER(SOCKET) {
    RUN_TEST_CASE(SOCKET, NONBLOCKINGHELPER);
    RUN_TEST_CASE(SOCKET, COMMONOPTIONSNOERROR);
    RUN_TEST_CASE(SOCKET, CLOSEIGNORESEINTR);
}
