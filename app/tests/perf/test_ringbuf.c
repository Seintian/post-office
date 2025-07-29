#include "unity/unity_fixture.h"
#include "perf/ringbuf.h"
#include <errno.h>
#include <stdlib.h>


static perf_ringbuf_t *rb;

TEST_GROUP(RingBuf);

TEST_SETUP(RingBuf) {
    // reset cacheline to default before each test
    perf_ringbuf_set_cacheline(0);
}

TEST_TEAR_DOWN(RingBuf) {
    perf_ringbuf_destroy(&rb);
}

TEST(RingBuf, InvalidCapacity) {
    // non-power-of-two capacity
    rb = perf_ringbuf_create(3);
    TEST_ASSERT_NULL(rb);
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

TEST(RingBuf, ValidCreateDestroy) {
    perf_ringbuf_set_cacheline(128);
    rb = perf_ringbuf_create(8);
    TEST_ASSERT_NOT_NULL(rb);
}

TEST(RingBuf, EmptyDequeue) {
    rb = perf_ringbuf_create(4);
    TEST_ASSERT_NOT_NULL(rb);
    void *item;
    int rc = perf_ringbuf_dequeue(rb, &item);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

TEST(RingBuf, SingleEnqueueDequeue) {
    rb = perf_ringbuf_create(4);
    TEST_ASSERT_NOT_NULL(rb);
    int value = 42;
    void *out;
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &value));
    TEST_ASSERT_EQUAL_UINT(1, perf_ringbuf_count(rb));
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(rb, &out));
    TEST_ASSERT_EQUAL_PTR(&value, out);
    TEST_ASSERT_EQUAL_UINT(0, perf_ringbuf_count(rb));
}

TEST(RingBuf, FullBuffer) {
    size_t cap = 4;
    rb = perf_ringbuf_create(cap);
    TEST_ASSERT_NOT_NULL(rb);
    int vals[4] = {1,2,3,4};
    // can hold cap-1 items = 3
    for (int i = 0; i < (int)(cap-1); i++) {
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &vals[i]));
    }
    // now full
    TEST_ASSERT_EQUAL_INT(-1, perf_ringbuf_enqueue(rb, &vals[3]));
    TEST_ASSERT_EQUAL_UINT(cap-1, perf_ringbuf_count(rb));
}

TEST(RingBuf, WrapAround) {
    size_t cap = 8;
    rb = perf_ringbuf_create(cap);
    TEST_ASSERT_NOT_NULL(rb);
    int vals[10];
    void *out;
    // enqueue 6 items
    for (int i = 0; i < 6; i++) {
        vals[i] = i;
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &vals[i]));
    }
    // dequeue 4 items
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(rb, &out));
        TEST_ASSERT_EQUAL_INT(i, *(int*)out);
    }
    // enqueue 4 more, wrap head
    for (int i = 6; i < 10; i++) {
        vals[i] = i;
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &vals[i]));
    }
    // now dequeue all remaining (total 6): values 4,5,6,7,8,9
    for (int j = 0; j < 6; j++) {
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(rb, &out));
        TEST_ASSERT_EQUAL_INT(j+4, *(int*)out);
    }
    TEST_ASSERT_EQUAL_UINT(0, perf_ringbuf_count(rb));
}

TEST(RingBuf, CountAccuracy) {
    rb = perf_ringbuf_create(16);
    TEST_ASSERT_NOT_NULL(rb);
    int x;
    // empty
    TEST_ASSERT_EQUAL_UINT(0, perf_ringbuf_count(rb));
    // enqueue 5
    for (int i = 0; i < 5; i++) perf_ringbuf_enqueue(rb, &x);
    TEST_ASSERT_EQUAL_UINT(5, perf_ringbuf_count(rb));
    // dequeue 2
    for (int i = 0; i < 2; i++) {
        void *out;
        perf_ringbuf_dequeue(rb, &out);
    }
    TEST_ASSERT_EQUAL_UINT(3, perf_ringbuf_count(rb));
    // enqueue 2 more
    for (int i = 0; i < 2; i++) perf_ringbuf_enqueue(rb, &x);
    TEST_ASSERT_EQUAL_UINT(5, perf_ringbuf_count(rb));
}

TEST_GROUP_RUNNER(RingBuf) {
    RUN_TEST_CASE(RingBuf, InvalidCapacity);
    RUN_TEST_CASE(RingBuf, ValidCreateDestroy);
    RUN_TEST_CASE(RingBuf, EmptyDequeue);
    RUN_TEST_CASE(RingBuf, SingleEnqueueDequeue);
    RUN_TEST_CASE(RingBuf, FullBuffer);
    RUN_TEST_CASE(RingBuf, WrapAround);
    RUN_TEST_CASE(RingBuf, CountAccuracy);
}
