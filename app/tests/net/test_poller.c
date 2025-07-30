#include "unity/unity_fixture.h"
#include "net/poller.h"
#include "perf/zerocopy.h"
#include "perf/ringbuf.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

TEST_GROUP(Poller);

static perf_zcpool_t *pool;
static poller_t      *p;

TEST_SETUP(Poller) {
    // pool of 4 event objects, sized for poller_event_t
    pool = perf_zcpool_create(4, sizeof(poller_event_t));
    TEST_ASSERT_NOT_NULL(pool);
    p = NULL;
}

TEST_TEAR_DOWN(Poller) {
    if (p) {
        poller_free(&p);
        TEST_ASSERT_NULL(p);
    }
    perf_zcpool_destroy(&pool);
    TEST_ASSERT_NULL(pool);
}

// --- Creation / Destruction ---

TEST(Poller, NewValid) {
    p = poller_new(8, pool);
    TEST_ASSERT_NOT_NULL(p);
}

TEST(Poller, NewInvalidParams) {
    // non‑power‑of‑two
    TEST_ASSERT_NULL(poller_new(3, pool));
}

// --- poller_ctl ---

TEST(Poller, CtlAddAndDel) {
    p = poller_new(4, pool);
    TEST_ASSERT_NOT_NULL(p);

    int efd = eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
    TEST_ASSERT_TRUE(efd >= 0);

    // add
    TEST_ASSERT_EQUAL_INT(0, poller_ctl(p, efd, EPOLLIN, (void*)(intptr_t)123, EPOLL_CTL_ADD));
    // modify
    TEST_ASSERT_EQUAL_INT(0, poller_ctl(p, efd, EPOLLIN|EPOLLOUT, (void*)(intptr_t)456, EPOLL_CTL_MOD));
    // delete
    TEST_ASSERT_EQUAL_INT(0, poller_ctl(p, efd, 0, NULL, EPOLL_CTL_DEL));

    close(efd);
}

TEST(Poller, CtlInvalid) {
    p = poller_new(4, pool);
    TEST_ASSERT_NOT_NULL(p);
    // bad op
    TEST_ASSERT_EQUAL_INT(-1, poller_ctl(p, 0, 0, NULL, 123));
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

// --- poller_wait / poller_next ---

TEST(Poller, WaitNoEvents) {
    p = poller_new(4, pool);
    TEST_ASSERT_NOT_NULL(p);

    // create but don't signal
    int efd = eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
    TEST_ASSERT_TRUE(efd >= 0);
    TEST_ASSERT_EQUAL_INT(0, poller_ctl(p, efd, EPOLLIN, (void*)0xdeadbeef, EPOLL_CTL_ADD));

    // should get no queued events
    int n = poller_wait(p, 10);
    TEST_ASSERT_EQUAL_INT(0, n);

    // next yields none
    poller_event_t *ev;
    TEST_ASSERT_EQUAL_INT(0, poller_next(p, &ev));
    close(efd);
}

TEST(Poller, WaitAndNext) {
    p = poller_new(4, pool);
    TEST_ASSERT_NOT_NULL(p);

    int efd = eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
    TEST_ASSERT_TRUE(efd >= 0);
    // register with a user pointer
    void *tag = (void*)0xfeedface;
    TEST_ASSERT_EQUAL_INT(0, poller_ctl(p, efd, EPOLLIN, tag, EPOLL_CTL_ADD));

    // signal
    uint64_t one = 1;
    if (write(efd, &one, sizeof(one)) < 0) {
        close(efd);
        TEST_FAIL_MESSAGE("Failed to write to eventfd");
    }

    int n = poller_wait(p, 100);
    TEST_ASSERT_EQUAL_INT(1, n);

    poller_event_t *ev;
    int rc = poller_next(p, &ev);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_PTR(tag, ev->user_data);
    TEST_ASSERT_TRUE(ev->events & EPOLLIN);
    // release back to pool
    perf_zcpool_release(pool, ev);

    // now empty
    TEST_ASSERT_EQUAL_INT(0, poller_next(p, &ev));
    close(efd);
}

// --- pool exhaustion ---

TEST(Poller, PoolExhaust) {
    // pool of only 1 element
    perf_zcpool_destroy(&pool);
    pool = perf_zcpool_create(1, sizeof(poller_event_t));
    TEST_ASSERT_NOT_NULL(pool);

    p = poller_new(4, pool);
    TEST_ASSERT_NOT_NULL(p);

    int efd = eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
    TEST_ASSERT_TRUE(efd >= 0);
    TEST_ASSERT_EQUAL_INT(0, poller_ctl(p, efd, EPOLLIN, (void*)0x1, EPOLL_CTL_ADD));

    // signal twice
    uint64_t inc = 1;
    if (write(efd, &inc, sizeof(inc)) < 0) {
        close(efd);
        TEST_FAIL_MESSAGE("Failed to write to eventfd");
    }
    if (write(efd, &inc, sizeof(inc)) < 0) {
        close(efd);
        TEST_FAIL_MESSAGE("Failed to write to eventfd");
    }
    if (write(efd, &inc, sizeof(inc)) < 0) {
        close(efd);
        TEST_FAIL_MESSAGE("Failed to write to eventfd");
    }

    // first wait: will queue one, then pool empty → errno=EAGAIN
    errno = 0;
    int n = poller_wait(p, 100);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(EAGAIN, errno);

    // retrieve and release
    poller_event_t *ev;
    TEST_ASSERT_EQUAL_INT(1, poller_next(p, &ev));
    perf_zcpool_release(pool, ev);

    // now second event can be queued
    errno = 0;
    n = poller_wait(p, 100);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(0, errno);

    // clean up
    TEST_ASSERT_EQUAL_INT(1, poller_next(p, &ev));
    perf_zcpool_release(pool, ev);

    close(efd);
}

// --- queue overflow ---

TEST(Poller, QueueOverflow) {
    // ring capacity = 2
    poller_free(&p);
    p = poller_new(2, pool);
    TEST_ASSERT_NOT_NULL(p);

    int fds[3];
    for (int i = 0; i < 3; i++) {
        fds[i] = eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
        TEST_ASSERT_TRUE(fds[i] >= 0);
        TEST_ASSERT_EQUAL_INT(0, poller_ctl(p, fds[i], EPOLLIN, (void*)(intptr_t)i, EPOLL_CTL_ADD));
        uint64_t v = 1;
        if (write(fds[i], &v, sizeof(v)) < 0) {
            close(fds[i]);
            TEST_FAIL_MESSAGE("Failed to write to eventfd");
        }
    }

    // first wait: can queue at most 2 events
    errno = 0;
    int n = poller_wait(p, 100);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_INT(ENOSPC, errno);

    // retrieve two
    poller_event_t *ev;
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_EQUAL_INT(1, poller_next(p, &ev));
        perf_zcpool_release(pool, ev);
    }

    // the third event was dropped; nothing more
    TEST_ASSERT_EQUAL_INT(0, poller_next(p, &ev));

    for (int i = 0; i < 3; i++) close(fds[i]);
}

// --- free and use-after-free ---

TEST(Poller, FreeAndNull) {
    p = poller_new(4, pool);
    TEST_ASSERT_NOT_NULL(p);
    poller_free(&p);
    TEST_ASSERT_NULL(p);
    // further ops fail
    TEST_ASSERT_EQUAL_INT(-1, poller_wait(p, 0));
}

TEST_GROUP_RUNNER(Poller) {
    RUN_TEST_CASE(Poller, NewValid);
    RUN_TEST_CASE(Poller, NewInvalidParams);
    RUN_TEST_CASE(Poller, CtlAddAndDel);
    RUN_TEST_CASE(Poller, CtlInvalid);
    RUN_TEST_CASE(Poller, WaitNoEvents);
    RUN_TEST_CASE(Poller, WaitAndNext);
    RUN_TEST_CASE(Poller, PoolExhaust);
    RUN_TEST_CASE(Poller, QueueOverflow);
    RUN_TEST_CASE(Poller, FreeAndNull);
}
