#include "unity/unity_fixture.h"
#include "postoffice/vector/vector.h"
#include <string.h>

TEST_GROUP(VECTOR);

static po_vector_t *vec;

TEST_SETUP(VECTOR) {
    vec = po_vector_create();
    TEST_ASSERT_NOT_NULL(vec);
}

TEST_TEAR_DOWN(VECTOR) {
    po_vector_destroy(vec);
}

TEST(VECTOR, CreateAndDestroy) {
    po_vector_t *v = po_vector_create();
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE(po_vector_is_empty(v));
    TEST_ASSERT_EQUAL_UINT(0, po_vector_size(v));
    po_vector_destroy(v);
}

TEST(VECTOR, CreateSized) {
    po_vector_t *v = po_vector_create_sized(32);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE(po_vector_capacity(v) >= 32);
    po_vector_destroy(v);
}

TEST(VECTOR, PushAndPop) {
    char *e1 = "elem1";
    char *e2 = "elem2";

    TEST_ASSERT_EQUAL_INT(0, po_vector_push(vec, e1));
    TEST_ASSERT_EQUAL_UINT(1, po_vector_size(vec));
    TEST_ASSERT_EQUAL_INT(0, po_vector_push(vec, e2));
    TEST_ASSERT_EQUAL_UINT(2, po_vector_size(vec));

    TEST_ASSERT_EQUAL_PTR(e2, po_vector_pop(vec));
    TEST_ASSERT_EQUAL_UINT(1, po_vector_size(vec));
    TEST_ASSERT_EQUAL_PTR(e1, po_vector_pop(vec));
    TEST_ASSERT_EQUAL_UINT(0, po_vector_size(vec));
    TEST_ASSERT_NULL(po_vector_pop(vec));
}

TEST(VECTOR, At) {
    po_vector_push(vec, "0");
    po_vector_push(vec, "1");
    po_vector_push(vec, "2");

    TEST_ASSERT_EQUAL_STRING("0", (char*)po_vector_at(vec, 0));
    TEST_ASSERT_EQUAL_STRING("1", (char*)po_vector_at(vec, 1));
    TEST_ASSERT_EQUAL_STRING("2", (char*)po_vector_at(vec, 2));
    TEST_ASSERT_NULL(po_vector_at(vec, 3));
    TEST_ASSERT_NULL(po_vector_at(vec, 100));
}

TEST(VECTOR, Insert) {
    po_vector_push(vec, "A");
    po_vector_push(vec, "C");

    TEST_ASSERT_EQUAL_INT(0, po_vector_insert(vec, 1, "B"));
    TEST_ASSERT_EQUAL_UINT(3, po_vector_size(vec));
    TEST_ASSERT_EQUAL_STRING("A", (char*)po_vector_at(vec, 0));
    TEST_ASSERT_EQUAL_STRING("B", (char*)po_vector_at(vec, 1));
    TEST_ASSERT_EQUAL_STRING("C", (char*)po_vector_at(vec, 2));

    // Insert at end
    TEST_ASSERT_EQUAL_INT(0, po_vector_insert(vec, 3, "D"));
    TEST_ASSERT_EQUAL_STRING("D", (char*)po_vector_at(vec, 3));

    // Invalid index
    TEST_ASSERT_NOT_EQUAL(0, po_vector_insert(vec, 10, "X"));
}

TEST(VECTOR, Remove) {
    po_vector_push(vec, "A"); // 0
    po_vector_push(vec, "B"); // 1
    po_vector_push(vec, "C"); // 2

    void *removed = po_vector_remove(vec, 1);
    TEST_ASSERT_EQUAL_STRING("B", (char*)removed);
    TEST_ASSERT_EQUAL_UINT(2, po_vector_size(vec));
    TEST_ASSERT_EQUAL_STRING("A", (char*)po_vector_at(vec, 0));
    TEST_ASSERT_EQUAL_STRING("C", (char*)po_vector_at(vec, 1));

    TEST_ASSERT_NULL(po_vector_remove(vec, 10));
}

TEST(VECTOR, ReserveAndShrink) {
    // Reserve
    TEST_ASSERT_EQUAL_INT(0, po_vector_reserve(vec, 100));
    TEST_ASSERT_TRUE(po_vector_capacity(vec) >= 100);

    // Shrink
    po_vector_push(vec, "X");
    TEST_ASSERT_EQUAL_INT(0, po_vector_shrink_to_fit(vec));
    TEST_ASSERT_TRUE(po_vector_capacity(vec) >= 1);
    // Ideally capacity should be strictly related to size now, but implementation detail may vary.
    // At least it should handle cleanup.
}

static int compare_str(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

TEST(VECTOR, SORT) {
    char *s1 = "C";
    char *s2 = "A";
    char *s3 = "B";

    po_vector_push(vec, s1);
    po_vector_push(vec, s2);
    po_vector_push(vec, s3);

    po_vector_sort(vec, compare_str);

    TEST_ASSERT_EQUAL_STRING("A", (char*)po_vector_at(vec, 0));
    TEST_ASSERT_EQUAL_STRING("B", (char*)po_vector_at(vec, 1));
    TEST_ASSERT_EQUAL_STRING("C", (char*)po_vector_at(vec, 2));
}

TEST(VECTOR, Copy) {
    po_vector_push(vec, "1");
    po_vector_push(vec, "2");

    po_vector_t *copy = po_vector_copy(vec);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_UINT(2, po_vector_size(copy));
    TEST_ASSERT_EQUAL_STRING("1", (char*)po_vector_at(copy, 0));
    TEST_ASSERT_EQUAL_STRING("2", (char*)po_vector_at(copy, 1));

    po_vector_destroy(copy);
}

TEST(VECTOR, Iterator) {
    po_vector_push(vec, "one");
    po_vector_push(vec, "two");

    po_vector_iter_t *iter = po_vector_iter_create(vec);
    TEST_ASSERT_NOT_NULL(iter);

    TEST_ASSERT_TRUE(po_vector_iter_has_next(iter));
    TEST_ASSERT_EQUAL_STRING("one", (char*)po_vector_next(iter));
    TEST_ASSERT_TRUE(po_vector_iter_has_next(iter));
    TEST_ASSERT_EQUAL_STRING("two", (char*)po_vector_next(iter));

    TEST_ASSERT_FALSE(po_vector_iter_has_next(iter));
    TEST_ASSERT_NULL(po_vector_next(iter));

    po_vector_iter_destroy(iter);
}

TEST_GROUP_RUNNER(VECTOR) {
    RUN_TEST_CASE(VECTOR, CreateAndDestroy);
    RUN_TEST_CASE(VECTOR, CreateSized);
    RUN_TEST_CASE(VECTOR, PushAndPop);
    RUN_TEST_CASE(VECTOR, At);
    RUN_TEST_CASE(VECTOR, Insert);
    RUN_TEST_CASE(VECTOR, Remove);
    RUN_TEST_CASE(VECTOR, ReserveAndShrink);
    RUN_TEST_CASE(VECTOR, SORT);
    RUN_TEST_CASE(VECTOR, Copy);
    RUN_TEST_CASE(VECTOR, Iterator);
}
