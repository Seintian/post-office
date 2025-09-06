#include <errno.h>
#include <stdlib.h>

#include "perf/ringbuf.h"
#include "unity/unity_fixture.h"

static po_perf_ringbuf_t *rb;

TEST_GROUP(RINGBUF);

TEST_SETUP(RINGBUF) {
    perf_ringbuf_set_cacheline(64);
    rb = perf_ringbuf_create(4); // 4 (power-of-two) -> 3 usable slots
    TEST_ASSERT_NOT_NULL(rb);
}

TEST_TEAR_DOWN(RINGBUF) {
    perf_ringbuf_destroy(&rb);
    TEST_ASSERT_NULL(rb);
}

TEST(RINGBUF, INVALID_CAPACITY) {
    // non-power-of-two capacity
    po_perf_ringbuf_t *_rb = perf_ringbuf_create(3);
    TEST_ASSERT_NULL(_rb);
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
    perf_ringbuf_destroy(&_rb);
    TEST_ASSERT_NULL(_rb);
}

TEST(RINGBUF, VALID_CREATE_DESTROY) {
    perf_ringbuf_set_cacheline(128);
    po_perf_ringbuf_t *_rb = perf_ringbuf_create(8);
    TEST_ASSERT_NOT_NULL(_rb);
    perf_ringbuf_destroy(&_rb);
    TEST_ASSERT_NULL(_rb);
}

TEST(RINGBUF, EMPTY_DEQUEUE) {
    void *item;
    int rc = perf_ringbuf_dequeue(rb, &item);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

TEST(RINGBUF, SINGLE_ENQUEUE_DEQUEUE) {
    int value = 42;
    void *out;
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &value));
    TEST_ASSERT_EQUAL_UINT(1, perf_ringbuf_count(rb));
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(rb, &out));
    TEST_ASSERT_EQUAL_PTR(&value, out);
    TEST_ASSERT_EQUAL_UINT(0, perf_ringbuf_count(rb));
}

TEST(RINGBUF, FULL_BUFFER) {
    size_t cap = 4;

    int vals[4] = {1, 2, 3, 4};
    // can hold cap-1 items = 3
    for (int i = 0; i < (int)(cap - 1); i++) {
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &vals[i]));
    }
    // now full
    TEST_ASSERT_EQUAL_INT(-1, perf_ringbuf_enqueue(rb, &vals[3]));
    TEST_ASSERT_EQUAL_UINT(cap - 1, perf_ringbuf_count(rb));
}

TEST(RINGBUF, WRAP_AROUND) {
    int vals[10];
    void *out;

    po_perf_ringbuf_t *_rb = perf_ringbuf_create(8); // 8 slots, 7 usable
    TEST_ASSERT_NOT_NULL(_rb);

    // enqueue 6 items
    for (int i = 0; i < 6; i++) {
        vals[i] = i;
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(_rb, &vals[i]));
    }
    // dequeue 4 items
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(_rb, &out));
        TEST_ASSERT_EQUAL_INT(i, *(int *)out);
    }
    // enqueue 4 more, wrap head
    for (int i = 6; i < 10; i++) {
        vals[i] = i;
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(_rb, &vals[i]));
    }
    // now dequeue all remaining (total 6): values 4,5,6,7,8,9
    for (int j = 0; j < 6; j++) {
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(_rb, &out));
        TEST_ASSERT_EQUAL_INT(j + 4, *(int *)out);
    }
    TEST_ASSERT_EQUAL_UINT(0, perf_ringbuf_count(_rb));

    perf_ringbuf_destroy(&_rb);
    TEST_ASSERT_NULL(_rb);
}

TEST(RINGBUF, COUNT_ACCURACY) {
    int x;

    po_perf_ringbuf_t *_rb = perf_ringbuf_create(8);
    TEST_ASSERT_NOT_NULL(_rb);
    // empty
    TEST_ASSERT_EQUAL_UINT(0, perf_ringbuf_count(_rb));
    // enqueue 5
    for (int i = 0; i < 5; i++) {
        perf_ringbuf_enqueue(_rb, &x);
    }
    TEST_ASSERT_EQUAL_UINT(5, perf_ringbuf_count(_rb));
    // dequeue 2
    for (int i = 0; i < 2; i++) {
        void *out;
        perf_ringbuf_dequeue(_rb, &out);
    }
    TEST_ASSERT_EQUAL_UINT(3, perf_ringbuf_count(_rb));
    // enqueue 2 more
    for (int i = 0; i < 2; i++) {
        perf_ringbuf_enqueue(_rb, &x);
    }
    TEST_ASSERT_EQUAL_UINT(5, perf_ringbuf_count(_rb));

    perf_ringbuf_destroy(&_rb);
    TEST_ASSERT_NULL(_rb);
}

// Basic enqueue/dequeue behavior
TEST(RINGBUF, ENQUEUE_DEQUEUE) {
    int a = 1;
    int b = 2;
    int c = 3;
    int *p;

    TEST_ASSERT_EQUAL_UINT64(0, perf_ringbuf_count(rb));

    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &a));
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &b));
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &c));
    // now full (3 of 3)
    TEST_ASSERT_EQUAL_INT(-1, perf_ringbuf_enqueue(rb, &a));
    TEST_ASSERT_EQUAL_UINT64(3, perf_ringbuf_count(rb));

    // dequeue in FIFO order
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(rb, (void **)&p));
    TEST_ASSERT_EQUAL_PTR(&a, p);
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(rb, (void **)&p));
    TEST_ASSERT_EQUAL_PTR(&b, p);
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(rb, (void **)&p));
    TEST_ASSERT_EQUAL_PTR(&c, p);
    // now empty
    TEST_ASSERT_EQUAL_INT(-1, perf_ringbuf_dequeue(rb, (void **)&p));
    TEST_ASSERT_EQUAL_UINT64(0, perf_ringbuf_count(rb));
}

// Peek without removing
TEST(RINGBUF, PEEK) {
    int x = 42;
    int y = 43;
    int *out;

    // empty peek fails
    TEST_ASSERT_EQUAL_INT(-1, perf_ringbuf_peek(rb, (void **)&out));

    perf_ringbuf_enqueue(rb, &x);
    perf_ringbuf_enqueue(rb, &y);
    // count == 2
    TEST_ASSERT_EQUAL_UINT64(2, perf_ringbuf_count(rb));

    // peek twice still same head, count unchanged
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_peek(rb, (void **)&out));
    TEST_ASSERT_EQUAL_PTR(&x, out);
    TEST_ASSERT_EQUAL_UINT64(2, perf_ringbuf_count(rb));
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_peek(rb, (void **)&out));
    TEST_ASSERT_EQUAL_PTR(&x, out);
    TEST_ASSERT_EQUAL_UINT64(2, perf_ringbuf_count(rb));
}

// Peek at arbitrary offset
TEST(RINGBUF, PEEK_AT) {
    int v[3] = {10, 20, 30};
    int *out;

    // empty peek_at fails
    TEST_ASSERT_EQUAL_INT(-1, perf_ringbuf_peek_at(rb, 0, (void **)&out));

    perf_ringbuf_enqueue(rb, &v[0]);
    perf_ringbuf_enqueue(rb, &v[1]);
    perf_ringbuf_enqueue(rb, &v[2]);
    // buffer is full now (3/3)

    // valid offsets
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_peek_at(rb, 0, (void **)&out));
    TEST_ASSERT_EQUAL_PTR(&v[0], out);
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_peek_at(rb, 1, (void **)&out));
    TEST_ASSERT_EQUAL_PTR(&v[1], out);
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_peek_at(rb, 2, (void **)&out));
    TEST_ASSERT_EQUAL_PTR(&v[2], out);

    // out-of-range
    TEST_ASSERT_EQUAL_INT(-1, perf_ringbuf_peek_at(rb, 3, (void **)&out));
    TEST_ASSERT_EQUAL_INT(-1, perf_ringbuf_peek_at(rb, 100, (void **)&out));
}

// Advance (drop) multiple items
TEST(RINGBUF, ADVANCE) {
    int v[4] = {7, 8, 9, 10};
    int *out;

    // enqueue only 3 (max)
    perf_ringbuf_enqueue(rb, &v[0]);
    perf_ringbuf_enqueue(rb, &v[1]);
    perf_ringbuf_enqueue(rb, &v[2]);

    // advance 0 is no-op
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_advance(rb, 0));
    TEST_ASSERT_EQUAL_UINT64(3, perf_ringbuf_count(rb));

    // advance 2
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_advance(rb, 2));
    TEST_ASSERT_EQUAL_UINT64(1, perf_ringbuf_count(rb));

    // peek now shows the third element
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_peek(rb, (void **)&out));
    TEST_ASSERT_EQUAL_PTR(&v[2], out);

    // advance the rest
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_advance(rb, 1));
    TEST_ASSERT_EQUAL_UINT64(0, perf_ringbuf_count(rb));

    // cannot advance beyond count
    TEST_ASSERT_EQUAL_INT(-1, perf_ringbuf_advance(rb, 1));
}

// Combined peek/advance/enqueue/dequeue
TEST(RINGBUF, MIXED_OPERATIONS) {
    int data[5] = {1, 2, 3, 4, 5};
    int *out;

    // fill to 3 items
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &data[i]));
    }

    // drop first via dequeue and advance
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(rb, (void **)&out));
    TEST_ASSERT_EQUAL_PTR(&data[0], out);
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_advance(rb, 1)); // removes data[1]
    // now only data[2] remains
    TEST_ASSERT_EQUAL_UINT64(1, perf_ringbuf_count(rb));
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_peek(rb, (void **)&out));
    TEST_ASSERT_EQUAL_PTR(&data[2], out);

    // add two more
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &data[3]));
    TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_enqueue(rb, &data[4]));
    TEST_ASSERT_EQUAL_UINT64(3, perf_ringbuf_count(rb));

    // dequeue all
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(0, perf_ringbuf_dequeue(rb, (void **)&out));
    }
    TEST_ASSERT_EQUAL_UINT64(0, perf_ringbuf_count(rb));
}

TEST_GROUP_RUNNER(RINGBUF) {
    RUN_TEST_CASE(RINGBUF, INVALID_CAPACITY);
    RUN_TEST_CASE(RINGBUF, VALID_CREATE_DESTROY);
    RUN_TEST_CASE(RINGBUF, EMPTY_DEQUEUE);
    RUN_TEST_CASE(RINGBUF, SINGLE_ENQUEUE_DEQUEUE);
    RUN_TEST_CASE(RINGBUF, FULL_BUFFER);
    RUN_TEST_CASE(RINGBUF, WRAP_AROUND);
    RUN_TEST_CASE(RINGBUF, COUNT_ACCURACY);
    RUN_TEST_CASE(RINGBUF, ENQUEUE_DEQUEUE);
    RUN_TEST_CASE(RINGBUF, PEEK);
    RUN_TEST_CASE(RINGBUF, PEEK_AT);
    RUN_TEST_CASE(RINGBUF, ADVANCE);
    RUN_TEST_CASE(RINGBUF, MIXED_OPERATIONS);
}
