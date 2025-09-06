// tests/hashset/test_hashset.c
#include <stdlib.h>
#include <string.h>

#include "hashset/hashset.h"
#include "unity/unity_fixture.h"

// Simple string hash and compare for tests
static unsigned long test_hash(const void *key) {
    const char *s = key;
    unsigned long h = 5381;
    int c;
    while ((c = *s++))
        h = ((h << 5) + h) + (unsigned char)c;

    return h;
}

static int test_cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

TEST_GROUP(HASHSET);

static hashset_t *set;

TEST_SETUP(HASHSET) {
    set = hashset_create_sized(test_cmp, test_hash, 5);
    TEST_ASSERT_NOT_NULL(set);
}

TEST_TEAR_DOWN(HASHSET) {
    hashset_destroy(&set);
}

TEST(HASHSET, CREATE_DEFAULT) {
    hashset_t *s2 = hashset_create(test_cmp, test_hash);
    TEST_ASSERT_NOT_NULL(s2);
    TEST_ASSERT_EQUAL_UINT(0, hashset_size(s2));
    TEST_ASSERT_TRUE(hashset_capacity(s2) >= 17);
    hashset_destroy(&s2);
}

TEST(HASHSET, ADD_AND_CONTAINS) {
    int rc = hashset_add(set, "a");
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(hashset_contains(set, "a"));
}

TEST(HASHSET, DUPLICATE_ADD) {
    hashset_add(set, "dup");
    int rc = hashset_add(set, "dup");
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT(1, hashset_size(set));
}

TEST(HASHSET, REMOVE) {
    hashset_add(set, "x");
    TEST_ASSERT_TRUE(hashset_contains(set, "x"));
    int rc = hashset_remove(set, "x");
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_FALSE(hashset_contains(set, "x"));
    rc = hashset_remove(set, "x");
    TEST_ASSERT_EQUAL_INT(0, rc);
}

TEST(HASHSET, SIZE_AND_CAPACITY) {
    TEST_ASSERT_EQUAL_UINT(0, hashset_size(set));
    size_t cap = hashset_capacity(set);
    hashset_add(set, "1");
    hashset_add(set, "2");
    TEST_ASSERT_EQUAL_UINT(2, hashset_size(set));
    TEST_ASSERT_EQUAL_UINT(cap, hashset_capacity(set));
}

TEST(HASHSET, KEYS_ARRAY) {
    hashset_add(set, "k1");
    hashset_add(set, "k2");
    void **keys = hashset_keys(set);
    TEST_ASSERT_NOT_NULL(keys);
    int found1 = 0;
    int found2 = 0;
    for (size_t i = 0; i < hashset_size(set); i++) {
        if (strcmp((char *)keys[i], "k1") == 0) {
            found1++;
        }
        if (strcmp((char *)keys[i], "k2") == 0) {
            found2++;
        }
    }
    free(keys);
    TEST_ASSERT_EQUAL_INT(1, found1);
    TEST_ASSERT_EQUAL_INT(1, found2);
}

TEST(HASHSET, CLEAR) {
    hashset_add(set, "c");
    TEST_ASSERT_EQUAL_UINT(1, hashset_size(set));
    hashset_clear(set);
    TEST_ASSERT_EQUAL_UINT(0, hashset_size(set));
    // capacity unchanged
    size_t cap = hashset_capacity(set);
    TEST_ASSERT_TRUE(cap >= 5);
}

TEST(HASHSET, LOAD_FACTOR) {
    float lf0 = hashset_load_factor(set);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lf0);
    hashset_add(set, "a");
    float lf1 = hashset_load_factor(set);
    TEST_ASSERT_TRUE(lf1 > 0.0f && lf1 <= 1.0f);
}

TEST(HASHSET, RESIZE_UP) {
    // small initial capacity triggers resize when many adds
    hashset_t *small = hashset_create_sized(test_cmp, test_hash, 3);
    TEST_ASSERT_NOT_NULL(small);
    size_t initial = hashset_capacity(small);
    for (int i = 0; i < 10; i++) {
        char *buf = malloc(13);
        snprintf(buf, 13, "k%d", i);
        hashset_add(small, buf);
    }
    size_t cap2 = hashset_capacity(small);
    TEST_ASSERT_TRUE(cap2 > initial);
    // cleanup keys
    void **keys = hashset_keys(small);
    for (size_t i = 0; i < hashset_size(small); i++) {
        free(keys[i]);
    }
    free(keys);
    hashset_destroy(&small);
}

TEST(HASHSET, RESIZE_DOWN) {
    hashset_t *s2 = hashset_create_sized(test_cmp, test_hash, 7);
    TEST_ASSERT_NOT_NULL(s2);
    // add few
    hashset_add(s2, "d1");
    hashset_add(s2, "d2");
    size_t cap1 = hashset_capacity(s2);
    // remove all
    hashset_remove(s2, "d1");
    hashset_remove(s2, "d2");
    size_t cap2 = hashset_capacity(s2);
    // capacity may shrink but never below initial
    TEST_ASSERT_TRUE(cap2 >= 17 || cap2 >= cap1);
    hashset_destroy(&s2);
}

TEST_GROUP_RUNNER(HASHSET) {
    RUN_TEST_CASE(HASHSET, CREATE_DEFAULT);
    RUN_TEST_CASE(HASHSET, ADD_AND_CONTAINS);
    RUN_TEST_CASE(HASHSET, DUPLICATE_ADD);
    RUN_TEST_CASE(HASHSET, REMOVE);
    RUN_TEST_CASE(HASHSET, SIZE_AND_CAPACITY);
    RUN_TEST_CASE(HASHSET, KEYS_ARRAY);
    RUN_TEST_CASE(HASHSET, CLEAR);
    RUN_TEST_CASE(HASHSET, LOAD_FACTOR);
    RUN_TEST_CASE(HASHSET, RESIZE_UP);
    RUN_TEST_CASE(HASHSET, RESIZE_DOWN);
}
