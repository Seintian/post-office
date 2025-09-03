#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity_fixture.h"
#include "utils/random.h"

TEST_GROUP(RANDOM);

TEST_SETUP(RANDOM) {
}
TEST_TEAR_DOWN(RANDOM) {
}

TEST(RANDOM, SEED_DETERMINISM) {
    po_rand_seed((uint64_t)0xDEADBEEFCAFEBABEULL);
    uint64_t a1 = po_rand_u64();
    uint64_t a2 = po_rand_u64();
    po_rand_seed((uint64_t)0xDEADBEEFCAFEBABEULL);
    uint64_t b1 = po_rand_u64();
    uint64_t b2 = po_rand_u64();
    TEST_ASSERT_EQUAL_UINT64(a1, b1);
    TEST_ASSERT_EQUAL_UINT64(a2, b2);
}

TEST(RANDOM, U32_AND_UNIT_RANGE) {
    po_rand_seed((uint64_t)123456789ULL);
    for (int i = 0; i < 1000; ++i) {
        uint32_t v = po_rand_u32();
        // No specific value check, just exercise the path
        (void)v;
        double d = po_rand_unit();
        TEST_ASSERT_TRUE(d >= 0.0);
        TEST_ASSERT_TRUE(d < 1.0);
    }
}

TEST(RANDOM, RANGE_I64_INCLUSIVE_AND_SWAP) {
    po_rand_seed((uint64_t)42ULL);
    // inclusive bounds
    int64_t min = -5, max = 5;
    for (int i = 0; i < 10000; ++i) {
        int64_t r = po_rand_range_i64(min, max);
        TEST_ASSERT_TRUE(r >= min && r <= max);
    }
    // swapped args should behave the same
    for (int i = 0; i < 10000; ++i) {
        int64_t r = po_rand_range_i64(max, min);
        TEST_ASSERT_TRUE(r >= min && r <= max);
    }
}

TEST(RANDOM, SHUFFLE_PERMUTES) {
    po_rand_seed((uint64_t)987654321ULL);
    enum { N = 10 };
    int arr[N];
    int orig[N];
    for (size_t i = 0; i < (size_t)N; ++i)
        arr[i] = (int)i, orig[i] = (int)i;
    po_rand_shuffle(arr, (size_t)N, sizeof(arr[0]));

    // Same multiset of elements
    int counts[N];
    memset(counts, 0, sizeof(counts));
    for (size_t i = 0; i < (size_t)N; ++i) {
        TEST_ASSERT_TRUE(arr[i] >= 0 && arr[i] < (int)N);
        counts[arr[i]]++;
    }
    for (size_t i = 0; i < (size_t)N; ++i) {
        TEST_ASSERT_EQUAL_INT(1, counts[i]);
    }

    // At least one element moved (highly likely; check deterministically by re-seeding if equal)
    bool same = true;
    for (size_t i = 0; i < (size_t)N; ++i)
        if (arr[i] != orig[i]) {
            same = false;
            break;
        }
    if (same) {
        // Re-seed and shuffle again to break equality case (rare)
        po_rand_seed((uint64_t)2222ULL);
        po_rand_shuffle(arr, (size_t)N, sizeof(arr[0]));
        same = true;
        for (size_t i = 0; i < (size_t)N; ++i)
            if (arr[i] != orig[i]) {
                same = false;
                break;
            }
    }
    TEST_ASSERT_FALSE(same);
}

TEST_GROUP_RUNNER(RANDOM) {
    RUN_TEST_CASE(RANDOM, SEED_DETERMINISM);
    RUN_TEST_CASE(RANDOM, U32_AND_UNIT_RANGE);
    RUN_TEST_CASE(RANDOM, RANGE_I64_INCLUSIVE_AND_SWAP);
    RUN_TEST_CASE(RANDOM, SHUFFLE_PERMUTES);
}
