// tests/hashset/test_hashset.c
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "postoffice/hashset/hashset.h"
#include "unity/unity_fixture.h"

// Simple string hash and compare for tests (djb2)
static unsigned long test_hash(const void *key) {
    const unsigned char *s = key;
    unsigned long h = 5381;
    int c;
    while ((c = *s++))
        h = ((h << 5) + h) + (unsigned char)c;
    return h;
}

static int test_cmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

TEST_GROUP(HASHSET);
static po_hashset_t *set;

TEST_SETUP(HASHSET) {
    set = po_hashset_create_sized(test_cmp, test_hash, 5);
    TEST_ASSERT_NOT_NULL(set);
}

TEST_TEAR_DOWN(HASHSET) { po_hashset_destroy(&set); }

TEST(HASHSET, CREATE_DEFAULT) {
    po_hashset_t *s2 = po_hashset_create(test_cmp, test_hash);
    TEST_ASSERT_NOT_NULL(s2);
    TEST_ASSERT_EQUAL_UINT(0, po_hashset_size(s2));
    TEST_ASSERT_TRUE(po_hashset_capacity(s2) >= 17);
    po_hashset_destroy(&s2);
}

TEST(HASHSET, ADD_AND_CONTAINS) {
    int rc = po_hashset_add(set, "a");
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(po_hashset_contains(set, "a"));
    TEST_ASSERT_EQUAL_UINT(1, po_hashset_size(set));
}

TEST(HASHSET, DUPLICATE_ADD) {
    po_hashset_add(set, "dup");
    int rc = po_hashset_add(set, "dup");
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT(1, po_hashset_size(set));
}

TEST(HASHSET, REMOVE) {
    po_hashset_add(set, "x");
    TEST_ASSERT_TRUE(po_hashset_contains(set, "x"));
    int rc = po_hashset_remove(set, "x");
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_FALSE(po_hashset_contains(set, "x"));
}

TEST(HASHSET, SIZE_AND_CAPACITY) {
    TEST_ASSERT_EQUAL_UINT(0, po_hashset_size(set));
    size_t cap = po_hashset_capacity(set);
    po_hashset_add(set, "1");
    po_hashset_add(set, "2");
    TEST_ASSERT_EQUAL_UINT(2, po_hashset_size(set));
    TEST_ASSERT_EQUAL_UINT(cap, po_hashset_capacity(set));
}

TEST(HASHSET, KEYS_ARRAY) {
    po_hashset_add(set, "k1");
    po_hashset_add(set, "k2");
    void **keys = po_hashset_keys(set);
    TEST_ASSERT_NOT_NULL(keys);
    int found1 = 0, found2 = 0;
    for (size_t i = 0; i < po_hashset_size(set); i++) {
        if (strcmp((char *)keys[i], "k1") == 0)
            found1++;
        if (strcmp((char *)keys[i], "k2") == 0)
            found2++;
    }
    free(keys);
    TEST_ASSERT_EQUAL_INT(1, found1);
    TEST_ASSERT_EQUAL_INT(1, found2);
}

TEST(HASHSET, CLEAR) {
    po_hashset_add(set, "c");
    TEST_ASSERT_EQUAL_UINT(1, po_hashset_size(set));
    po_hashset_clear(set);
    TEST_ASSERT_EQUAL_UINT(0, po_hashset_size(set));
    size_t cap = po_hashset_capacity(set);
    TEST_ASSERT_TRUE(cap >= 5);
}

TEST(HASHSET, LOAD_FACTOR) {
    float lf0 = po_hashset_load_factor(set);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lf0);
    po_hashset_add(set, "a");
    float lf1 = po_hashset_load_factor(set);
    TEST_ASSERT_TRUE(lf1 > 0.0f && lf1 <= 1.0f);
}

TEST(HASHSET, RESIZE_UP) {
    po_hashset_t *small = po_hashset_create_sized(test_cmp, test_hash, 3);
    TEST_ASSERT_NOT_NULL(small);
    for (int i = 0; i < 10; i++) {
        char *buf = malloc(13);
        snprintf(buf, 13, "k%d", i);
        po_hashset_add(small, buf);
    }
    size_t cap2 = po_hashset_capacity(small);
    TEST_ASSERT_TRUE(cap2 > 3);
    // cleanup inserted keys
    void **keys = po_hashset_keys(small);
    for (size_t i = 0; i < po_hashset_size(small); i++)
        free(keys[i]);
    free(keys);
    po_hashset_destroy(&small);
}

TEST(HASHSET, RESIZE_DOWN) {
    po_hashset_t *s2 = po_hashset_create_sized(test_cmp, test_hash, 7);
    TEST_ASSERT_NOT_NULL(s2);
    po_hashset_add(s2, "d1");
    po_hashset_add(s2, "d2");
    size_t cap1 = po_hashset_capacity(s2);
    po_hashset_remove(s2, "d1");
    po_hashset_remove(s2, "d2");
    size_t cap2 = po_hashset_capacity(s2);
    TEST_ASSERT_TRUE(cap2 == cap1 || cap2 < cap1);
    po_hashset_destroy(&s2);
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

