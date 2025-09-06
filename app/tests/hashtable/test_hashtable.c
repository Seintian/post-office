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
static hashtable_t *ht;

TEST_SETUP(HASHTABLE) {
    ht = hashtable_create_sized(test_cmp, test_hash, 7);
    TEST_ASSERT_NOT_NULL(ht);
}

TEST_TEAR_DOWN(HASHTABLE) {
    hashtable_destroy(&ht);
}

TEST(HASHTABLE, CREATE_DEFAULT) {
    hashtable_t *h2 = hashtable_create(test_cmp, test_hash);
    TEST_ASSERT_NOT_NULL(h2);
    TEST_ASSERT_EQUAL_UINT(0, hashtable_size(h2));
    TEST_ASSERT_TRUE(hashtable_capacity(h2) >= 17);
    hashtable_destroy(&h2);
}

TEST(HASHTABLE, PUT_AND_GET) {
    int a = 1;
    int b = 2;
    int ret = hashtable_put(ht, "key1", &a);
    TEST_ASSERT_EQUAL_INT(1, ret);
    ret = hashtable_put(ht, "key1", &b);
    TEST_ASSERT_EQUAL_INT(0, ret);
    int *v = hashtable_get(ht, "key1");
    TEST_ASSERT_EQUAL_PTR(&b, v);
}

TEST(HASHTABLE, CONTAINS_KEY) {
    hashtable_put(ht, "k", "v");
    TEST_ASSERT_TRUE(hashtable_contains_key(ht, "k"));
    TEST_ASSERT_FALSE(hashtable_contains_key(ht, "x"));
}

TEST(HASHTABLE, REMOVE) {
    hashtable_put(ht, "r", "val");
    TEST_ASSERT_TRUE(hashtable_contains_key(ht, "r"));
    int removed = hashtable_remove(ht, "r");
    TEST_ASSERT_EQUAL_INT(1, removed);
    TEST_ASSERT_FALSE(hashtable_contains_key(ht, "r"));
    removed = hashtable_remove(ht, "r");
    TEST_ASSERT_EQUAL_INT(0, removed);
}

TEST(HASHTABLE, SIZE_AND_CAPACITY) {
    TEST_ASSERT_EQUAL_UINT(0, hashtable_size(ht));
    size_t cap = hashtable_capacity(ht);
    hashtable_put(ht, "a", "1");
    hashtable_put(ht, "b", "2");
    TEST_ASSERT_EQUAL_UINT(2, hashtable_size(ht));
    TEST_ASSERT_EQUAL_UINT(cap, hashtable_capacity(ht));
}

TEST(HASHTABLE, KEY_SET_AND_VALUES) {
    hashtable_put(ht, "k1", "v1");
    hashtable_put(ht, "k2", "v2");
    void **keys = hashtable_keyset(ht);
    TEST_ASSERT_NOT_NULL(keys);
    // two keys
    int count = 0;
    for (size_t i = 0; i < hashtable_size(ht); i++) {
        count++;
    }
    free(keys);
    // values
    void **vals = hashtable_values(ht);
    TEST_ASSERT_NOT_NULL(vals);
    count = 0;
    for (size_t i = 0; i < hashtable_size(ht); i++) {
        count++;
    }
    free(vals);
}

TEST(HASHTABLE, CLEAR) {
    hashtable_put(ht, "x", "y");
    TEST_ASSERT_EQUAL_UINT(1, hashtable_size(ht));
    int rc = hashtable_clear(ht);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_UINT(0, hashtable_size(ht));
    rc = hashtable_clear(ht);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

static int map_sum;
static void map_fn(void *key, void *value) {
    (void)key;
    map_sum += atoi((char *)value);
}

TEST(HASHTABLE, MAP) {
    map_sum = 0;
    hashtable_put(ht, "a", "1");
    hashtable_put(ht, "b", "2");
    hashtable_map(ht, map_fn);
    TEST_ASSERT_EQUAL_INT(3, map_sum);
}

TEST(HASHTABLE, LOAD_FACTOR) {
    float lf = hashtable_load_factor(ht);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lf);
    hashtable_put(ht, "a", "v");
    lf = hashtable_load_factor(ht);
    TEST_ASSERT_TRUE(lf > 0.0f);
}

TEST(HASHTABLE, REPLACE) {
    hashtable_put(ht, "k", "v1");
    int rc = hashtable_replace(ht, "k", "v2");
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_STRING("v2", (char *)hashtable_get(ht, "k"));
    rc = hashtable_replace(ht, "x", "v");
    TEST_ASSERT_EQUAL_INT(0, rc);
}

TEST(HASHTABLE, EQUALS_AND_COPY) {
    hashtable_put(ht, "1", "one");
    hashtable_put(ht, "2", "two");
    hashtable_t *copy = hashtable_copy(ht);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_TRUE(hashtable_equals(ht, copy, test_cmp));
    // modify copy
    hashtable_put(copy, "3", "three");
    TEST_ASSERT_FALSE(hashtable_equals(ht, copy, test_cmp));
    hashtable_destroy(&copy);
}

TEST(HASHTABLE, MERGE) {
    hashtable_t *src = hashtable_create_sized(test_cmp, test_hash, 5);
    hashtable_put(ht, "a", "1");
    hashtable_put(src, "b", "2");
    hashtable_merge(ht, src);
    TEST_ASSERT_EQUAL_STRING("2", (char *)hashtable_get(ht, "b"));
    hashtable_destroy(&src);
}

TEST(HASHTABLE, ITERATOR) {
    hashtable_put(ht, "x", "10");
    hashtable_put(ht, "y", "20");
    int count = 0;
    hashtable_iter_t *it = hashtable_iterator(ht);
    TEST_ASSERT_NOT_NULL(it);
    while (hashtable_iter_next(it)) {
        const void *k = hashtable_iter_key(it);
        const void *v = hashtable_iter_value(it);
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
