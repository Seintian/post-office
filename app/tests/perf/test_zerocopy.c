#include <errno.h>
#include <string.h>

#include "perf/zerocopy.h"
#include "unity/unity_fixture.h"

TEST_GROUP(ZEROCOPY);
static perf_zcpool_t *pool;

TEST_SETUP(ZEROCOPY) {
    // create a pool of 4 buffers, each 1024 bytes
    pool = perf_zcpool_create(4, 1024, PERF_ZCPOOL_NOFLAGS);
    TEST_ASSERT_NOT_NULL(pool);
    // ring of 4 can only hold 3 entries → freecount == 3
    TEST_ASSERT_EQUAL_UINT(3, perf_zcpool_freecount(pool));
}

TEST_TEAR_DOWN(ZEROCOPY) {
    perf_zcpool_destroy(&pool);
}

TEST(ZEROCOPY, INVALID_CREATE) {
    const perf_zcpool_t *p1 = perf_zcpool_create(0, 1024, PERF_ZCPOOL_NOFLAGS);
    TEST_ASSERT_NULL(p1);
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);

    const perf_zcpool_t *p2 = perf_zcpool_create(4, 0, PERF_ZCPOOL_NOFLAGS);
    TEST_ASSERT_NULL(p2);
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);

    const perf_zcpool_t *p3 = perf_zcpool_create(4, (2UL << 20) + 1, PERF_ZCPOOL_NOFLAGS);
    TEST_ASSERT_NULL(p3);
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

TEST(ZEROCOPY, ACQUIRE_RELEASE_BASIC) {
    void *bufs[4];
    // Only 3 buffers can actually be acquired
    for (int i = 0; i < 3; i++) {
        bufs[i] = perf_zcpool_acquire(pool);
        TEST_ASSERT_NOT_NULL(bufs[i]);
        // after i+1 acquisitions, freecount == 3 - (i+1)
        TEST_ASSERT_EQUAL_UINT(3 - (i + 1), perf_zcpool_freecount(pool));
        memset(bufs[i], i, 1024);
    }

    // further acquire should fail immediately
    const void *b = perf_zcpool_acquire(pool);
    TEST_ASSERT_NULL(b);
    TEST_ASSERT_EQUAL_INT(EAGAIN, errno);

    // release in a different order, one at a time
    perf_zcpool_release(pool, bufs[2]);
    TEST_ASSERT_EQUAL_UINT(1, perf_zcpool_freecount(pool));

    perf_zcpool_release(pool, bufs[0]);
    TEST_ASSERT_EQUAL_UINT(2, perf_zcpool_freecount(pool));

    perf_zcpool_release(pool, bufs[1]);
    TEST_ASSERT_EQUAL_UINT(3, perf_zcpool_freecount(pool));
}

TEST(ZEROCOPY, BUFFER_DISTINCTNESS) {
    // can still only get 3 pointers
    void *a = perf_zcpool_acquire(pool);
    void *b = perf_zcpool_acquire(pool);
    void *c = perf_zcpool_acquire(pool);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NOT_EQUAL(a, b);
    TEST_ASSERT_NOT_EQUAL(b, c);
    TEST_ASSERT_NOT_EQUAL(a, c);
    perf_zcpool_release(pool, a);
    perf_zcpool_release(pool, b);
    perf_zcpool_release(pool, c);
}

TEST(ZEROCOPY, RELEASE_INVALID) {
    // releasing NULL or foreign ptr should not crash
    int x;
    perf_zcpool_release(pool, &x);
    // freecount remains at 3
    TEST_ASSERT_EQUAL_UINT(3, perf_zcpool_freecount(pool));
}

TEST_GROUP_RUNNER(ZEROCOPY) {
    RUN_TEST_CASE(ZEROCOPY, INVALID_CREATE);
    RUN_TEST_CASE(ZEROCOPY, ACQUIRE_RELEASE_BASIC);
    RUN_TEST_CASE(ZEROCOPY, BUFFER_DISTINCTNESS);
    RUN_TEST_CASE(ZEROCOPY, RELEASE_INVALID);
}
