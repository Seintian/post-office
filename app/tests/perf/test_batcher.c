#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "perf/batcher.h"
#include "perf/ringbuf.h"
#include "unity/unity_fixture.h"

TEST_GROUP(BATCHER);
static perf_batcher_t *batcher;
static perf_ringbuf_t *ringbuf;

TEST_SETUP(BATCHER) {
    // create a ring with capacity 8 and batcher size 4
    ringbuf = perf_ringbuf_create(8);
    TEST_ASSERT_NOT_NULL(ringbuf);
    batcher = perf_batcher_create(ringbuf, 4);
    TEST_ASSERT_NOT_NULL(batcher);
}

TEST_TEAR_DOWN(BATCHER) {
    perf_batcher_destroy(&batcher);
    perf_ringbuf_destroy(&ringbuf);
}

TEST(BATCHER, INVALID_CREATE) {
    // zero batch size
    perf_ringbuf_t *rb = perf_ringbuf_create(8);
    TEST_ASSERT_NOT_NULL(rb);
    const perf_batcher_t *b2 = perf_batcher_create(rb, 0);
    TEST_ASSERT_NULL(b2);
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
    perf_ringbuf_destroy(&rb);
}

TEST(BATCHER, SINGLE_BATCH) {
    int v = 123;
    void *out[4];
    // enqueue one
    TEST_ASSERT_EQUAL_INT(0, perf_batcher_enqueue(batcher, &v));
    // next should return 1
    ssize_t n = perf_batcher_next(batcher, out);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_PTR(&v, out[0]);
}

TEST(BATCHER, PARTIAL_BATCH) {
    void *out[4];
    int vals[3] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(0, perf_batcher_enqueue(batcher, &vals[i]));
    }
    ssize_t n = perf_batcher_next(batcher, out);
    TEST_ASSERT_EQUAL_INT(3, n);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(vals[i], *(int *)out[i]);
    }
}

TEST(BATCHER, FULL_BATCH) {
    void *out[4];
    int vals[6] = {1, 2, 3, 4, 5, 6};
    // enqueue 6, batcher size is 4
    for (int i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_INT(0, perf_batcher_enqueue(batcher, &vals[i]));
    }
    ssize_t n1 = perf_batcher_next(batcher, out);
    TEST_ASSERT_EQUAL_INT(4, n1);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(vals[i], *(int *)out[i]);
    }
    // next should get remaining 2
    ssize_t n2 = perf_batcher_next(batcher, out);
    TEST_ASSERT_EQUAL_INT(2, n2);
    TEST_ASSERT_EQUAL_INT(vals[4], *(int *)out[0]);
    TEST_ASSERT_EQUAL_INT(vals[5], *(int *)out[1]);
}

// Optional: test blocking via thread
static void *consumer_thread(void *arg) {
    perf_batcher_t *b = arg;
    void *out[1];
    ssize_t n = perf_batcher_next(b, out);
    // should get exactly 1
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_NOT_NULL(out[0]);
    return NULL;
}

TEST(BATCHER, BLOCKING_NEXT) {
    // spawn consumer that will block until we enqueue
    pthread_t tid;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&tid, NULL, consumer_thread, batcher));
    // give thread a moment to block
    struct timespec ts = {0, 10000000}; // 10,000,000 ns = 10 ms
    nanosleep(&ts, NULL);
    int v = 77;
    TEST_ASSERT_EQUAL_INT(0, perf_batcher_enqueue(batcher, &v));
    // wait for consumer thread
    TEST_ASSERT_EQUAL_INT(0, pthread_join(tid, NULL));
}

TEST_GROUP_RUNNER(BATCHER) {
    RUN_TEST_CASE(BATCHER, INVALID_CREATE);
    RUN_TEST_CASE(BATCHER, SINGLE_BATCH);
    RUN_TEST_CASE(BATCHER, PARTIAL_BATCH);
    RUN_TEST_CASE(BATCHER, FULL_BATCH);
    RUN_TEST_CASE(BATCHER, BLOCKING_NEXT);
}
