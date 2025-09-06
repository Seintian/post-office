// tests/hashtable/test_hashtable.c
#include <stdlib.h>
#include <string.h>

#include "hashtable/hashtable.h"
#include "unity/unity_fixture.h"

// Simple string hash and compare for tests
static unsigned long test_hash(const void *key) {
    const char *s = key;
    unsigned long h = 5381;
    int c;
    while ((c = *s++)) {
        h = ((h << 5) + h) + (unsigned char)c;
    }
    return h;
}
static int test_cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

TEST_GROUP(HASHTABLE);
static po_hashtable_t *ht;

TEST_SETUP(HASHTABLE) {
    ht = po_hashtable_create_sized(test_cmp, test_hash, 7);
    TEST_ASSERT_NOT_NULL(ht);
}

TEST_TEAR_DOWN(HASHTABLE) {
    po_hashtable_destroy(&ht);
}

TEST(HASHTABLE, CREATE_DEFAULT) {
    po_hashtable_t *h2 = po_hashtable_create(test_cmp, test_hash);
    TEST_ASSERT_NOT_NULL(h2);
    TEST_ASSERT_EQUAL_UINT(0, po_hashtable_size(h2));
    TEST_ASSERT_TRUE(po_hashtable_capacity(h2) >= 17);
    po_hashtable_destroy(&h2);
}

TEST(HASHTABLE, PUT_AND_GET) {
    int a = 1;
    int b = 2;
    int ret = po_hashtable_put(ht, "key1", &a);
    TEST_ASSERT_EQUAL_INT(1, ret);
    ret = po_hashtable_put(ht, "key1", &b);
    TEST_ASSERT_EQUAL_INT(0, ret);
    int *v = po_hashtable_get(ht, "key1");
    TEST_ASSERT_EQUAL_PTR(&b, v);
}

TEST(HASHTABLE, CONTAINS_KEY) {
    po_hashtable_put(ht, "k", "v");
    TEST_ASSERT_TRUE(po_hashtable_contains_key(ht, "k"));
    TEST_ASSERT_FALSE(po_hashtable_contains_key(ht, "x"));
}

TEST(HASHTABLE, REMOVE) {
    po_hashtable_put(ht, "r", "val");
    TEST_ASSERT_TRUE(po_hashtable_contains_key(ht, "r"));
    int removed = po_hashtable_remove(ht, "r");
    TEST_ASSERT_EQUAL_INT(1, removed);
    TEST_ASSERT_FALSE(po_hashtable_contains_key(ht, "r"));
    removed = po_hashtable_remove(ht, "r");
    TEST_ASSERT_EQUAL_INT(0, removed);
}

TEST(HASHTABLE, SIZE_AND_CAPACITY) {
    TEST_ASSERT_EQUAL_UINT(0, po_hashtable_size(ht));
    size_t cap = po_hashtable_capacity(ht);
    po_hashtable_put(ht, "a", "1");
    po_hashtable_put(ht, "b", "2");
    TEST_ASSERT_EQUAL_UINT(2, po_hashtable_size(ht));
    TEST_ASSERT_EQUAL_UINT(cap, po_hashtable_capacity(ht));
}

TEST(HASHTABLE, KEY_SET_AND_VALUES) {
    po_hashtable_put(ht, "k1", "v1");
    po_hashtable_put(ht, "k2", "v2");
    void **keys = po_hashtable_keyset(ht);
    TEST_ASSERT_NOT_NULL(keys);
    // two keys
    int count = 0;
    for (size_t i = 0; i < po_hashtable_size(ht); i++) {
        count++;
    }
    free(keys);
    // values
    void **vals = po_hashtable_values(ht);
    TEST_ASSERT_NOT_NULL(vals);
    count = 0;
    for (size_t i = 0; i < po_hashtable_size(ht); i++) {
        count++;
    }
    free(vals);
}

TEST(HASHTABLE, CLEAR) {
    po_hashtable_put(ht, "x", "y");
    TEST_ASSERT_EQUAL_UINT(1, po_hashtable_size(ht));
    int rc = po_hashtable_clear(ht);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_UINT(0, po_hashtable_size(ht));
    rc = po_hashtable_clear(ht);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

static int map_sum;
static void map_fn(void *key, void *value) {
    (void)key;
    map_sum += atoi((char *)value);
}

TEST(HASHTABLE, MAP) {
    map_sum = 0;
    po_hashtable_put(ht, "a", "1");
    po_hashtable_put(ht, "b", "2");
    po_hashtable_map(ht, map_fn);
    TEST_ASSERT_EQUAL_INT(3, map_sum);
}

TEST(HASHTABLE, LOAD_FACTOR) {
    float lf = po_hashtable_load_factor(ht);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lf);
    po_hashtable_put(ht, "a", "v");
    lf = po_hashtable_load_factor(ht);
    TEST_ASSERT_TRUE(lf > 0.0f);
}

TEST(HASHTABLE, REPLACE) {
    po_hashtable_put(ht, "k", "v1");
    int rc = po_hashtable_replace(ht, "k", "v2");
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_STRING("v2", (char *)po_hashtable_get(ht, "k"));
    rc = po_hashtable_replace(ht, "x", "v");
    TEST_ASSERT_EQUAL_INT(0, rc);
}

TEST(HASHTABLE, EQUALS_AND_COPY) {
    po_hashtable_put(ht, "1", "one");
    po_hashtable_put(ht, "2", "two");
    po_hashtable_t *copy = po_hashtable_copy(ht);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_TRUE(po_hashtable_equals(ht, copy, test_cmp));
    // modify copy
    po_hashtable_put(copy, "3", "three");
    TEST_ASSERT_FALSE(po_hashtable_equals(ht, copy, test_cmp));
    po_hashtable_destroy(&copy);
}

TEST(HASHTABLE, MERGE) {
    po_hashtable_t *src = po_hashtable_create_sized(test_cmp, test_hash, 5);
    po_hashtable_put(ht, "a", "1");
    po_hashtable_put(src, "b", "2");
    po_hashtable_merge(ht, src);
    TEST_ASSERT_EQUAL_STRING("2", (char *)po_hashtable_get(ht, "b"));
    po_hashtable_destroy(&src);
}

TEST(HASHTABLE, ITERATOR) {
    po_hashtable_put(ht, "x", "10");
    po_hashtable_put(ht, "y", "20");
    int count = 0;
    po_hashtable_iter_t *it = po_hashtable_iterator(ht);
    TEST_ASSERT_NOT_NULL(it);
    while (po_hashtable_iter_next(it)) {
        const void *k = po_hashtable_iter_key(it);
        const void *v = po_hashtable_iter_value(it);
        TEST_ASSERT_NOT_NULL(k);
        TEST_ASSERT_NOT_NULL(v);
        count++;
    }
    TEST_ASSERT_EQUAL_INT(2, count);
    free(it);
}

TEST_GROUP_RUNNER(HASHTABLE) {
    RUN_TEST_CASE(HASHTABLE, CREATE_DEFAULT);
    RUN_TEST_CASE(HASHTABLE, PUT_AND_GET);
    RUN_TEST_CASE(HASHTABLE, CONTAINS_KEY);
    RUN_TEST_CASE(HASHTABLE, REMOVE);
    RUN_TEST_CASE(HASHTABLE, SIZE_AND_CAPACITY);
    RUN_TEST_CASE(HASHTABLE, KEY_SET_AND_VALUES);
    RUN_TEST_CASE(HASHTABLE, CLEAR);
    RUN_TEST_CASE(HASHTABLE, MAP);
    RUN_TEST_CASE(HASHTABLE, LOAD_FACTOR);
    RUN_TEST_CASE(HASHTABLE, REPLACE);
    RUN_TEST_CASE(HASHTABLE, EQUALS_AND_COPY);
    RUN_TEST_CASE(HASHTABLE, MERGE);
    RUN_TEST_CASE(HASHTABLE, ITERATOR);
}
