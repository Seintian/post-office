// tests/net/test_poller.c
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "net/poller.h"
#include "unity/unity_fixture.h"

TEST_GROUP(POLLER);

TEST_SETUP(POLLER) { /* no-op */
}
TEST_TEAR_DOWN(POLLER) { /* no-op */
}

TEST(POLLER, CREATE_AND_DESTROY) {
    poller_t *p = poller_create();
    TEST_ASSERT_NOT_NULL(p);
    poller_destroy(p);
}

TEST(POLLER, ADD_MODIFY_REMOVE_AND_WAIT) {
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

TEST(POLLER, WAIT_TIMEOUT) {
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

TEST(POLLER, WAKE_NO_EVENTS) {
    poller_t *p = poller_create();
    TEST_ASSERT_NOT_NULL(p);
    struct epoll_event ev[2];
    // Issue a wake and then wait with a long timeout; should return 0 (wake only) quickly.
    TEST_ASSERT_EQUAL_INT(0, poller_wake(p));
    int n = poller_wait(p, ev, 2, 1000);
    TEST_ASSERT_EQUAL_INT(0, n); // wake consumed, no external fds
    poller_destroy(p);
}

TEST(POLLER, TIMED_WAIT_WAKE_BEFORE_TIMEOUT) {
    poller_t *p = poller_create();
    TEST_ASSERT_NOT_NULL(p);
    struct epoll_event ev[4];
    bool timed_out = false;
    // Trigger wake before calling timed wait to simulate asynchronous wake
    TEST_ASSERT_EQUAL_INT(0, poller_wake(p));
    int n = poller_timed_wait(p, ev, 4, 200, &timed_out);
    TEST_ASSERT_EQUAL_INT(0, n); // wake only (no external events)
    TEST_ASSERT_FALSE(timed_out); // should not be reported as a timeout
    poller_destroy(p);
}

TEST_GROUP_RUNNER(POLLER) {
    RUN_TEST_CASE(POLLER, CREATE_AND_DESTROY);
    RUN_TEST_CASE(POLLER, ADD_MODIFY_REMOVE_AND_WAIT);
    RUN_TEST_CASE(POLLER, WAIT_TIMEOUT);
    RUN_TEST_CASE(POLLER, WAKE_NO_EVENTS);
    RUN_TEST_CASE(POLLER, TIMED_WAIT_WAKE_BEFORE_TIMEOUT);
}
