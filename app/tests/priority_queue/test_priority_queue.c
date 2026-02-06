/**
 * @file test_priority_queue.c
 * @brief Exhaustive test suite for the priority_queue library.
 *
 * Tests cover:
 * - Creation and destruction
 * - Push, pop, peek operations
 * - Arbitrary removal
 * - Size and is_empty checks
 * - Heap ordering properties
 * - Edge cases (NULL inputs, duplicate elements, single element)
 * - Stress tests with many elements
 * - Integration with custom comparators
 */

#include <postoffice/priority_queue/priority_queue.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity_fixture.h"

// --- Test Utilities ---

// Integer comparison: Min-Heap (smallest first)
static int cmp_int_min(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return ia - ib;
}

// Integer comparison: Max-Heap (largest first)
static int cmp_int_max(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return ib - ia;
}

// String comparison
static int cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

// Simple hash for integers (pointer identity)
static unsigned long hash_ptr(const void *a) {
    return (unsigned long)(uintptr_t)a;
}

// String hash (djb2)
static unsigned long hash_str(const void *a) {
    const char *s = a;
    unsigned long h = 5381;
    int c;
    while ((c = *s++))
        h = ((h << 5) + h) + (unsigned char)c;
    return h;
}

// --- Test Group Definition ---

TEST_GROUP(PRIORITY_QUEUE);

static po_priority_queue_t *pq;
static int test_ints[1024];

TEST_SETUP(PRIORITY_QUEUE) {
    pq = po_priority_queue_create(cmp_int_min, hash_ptr);
    TEST_ASSERT_NOT_NULL(pq);
    for (int i = 0; i < 1024; i++) {
        test_ints[i] = i;
    }
}

TEST_TEAR_DOWN(PRIORITY_QUEUE) {
    if (pq) {
        po_priority_queue_destroy(pq);
        pq = NULL;
    }
}

// ============================================================================
// Unit Tests: Creation and Destruction
// ============================================================================

TEST(PRIORITY_QUEUE, CreateAndDestroy) {
    po_priority_queue_t *q = po_priority_queue_create(cmp_int_min, hash_ptr);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_TRUE(po_priority_queue_is_empty(q));
    TEST_ASSERT_EQUAL_UINT(0, po_priority_queue_size(q));
    po_priority_queue_destroy(q);
}

TEST(PRIORITY_QUEUE, CreateWithNullComparator) {
    po_priority_queue_t *q = po_priority_queue_create(NULL, hash_ptr);
    TEST_ASSERT_NULL(q);
}

TEST(PRIORITY_QUEUE, CreateWithNullHash) {
    po_priority_queue_t *q = po_priority_queue_create(cmp_int_min, NULL);
    TEST_ASSERT_NULL(q);
}

TEST(PRIORITY_QUEUE, DestroyNull) {
    // Should not crash
    po_priority_queue_destroy(NULL);
}

// ============================================================================
// Unit Tests: Push Operations
// ============================================================================

TEST(PRIORITY_QUEUE, PushSingle) {
    int rc = po_priority_queue_push(pq, &test_ints[42]);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT(1, po_priority_queue_size(pq));
    TEST_ASSERT_FALSE(po_priority_queue_is_empty(pq));
}

TEST(PRIORITY_QUEUE, PushMultiple) {
    for (int i = 0; i < 10; i++) {
        int rc = po_priority_queue_push(pq, &test_ints[i]);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }
    TEST_ASSERT_EQUAL_UINT(10, po_priority_queue_size(pq));
}

TEST(PRIORITY_QUEUE, PushNullElement) {
    int rc = po_priority_queue_push(pq, NULL);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_UINT(0, po_priority_queue_size(pq));
}

TEST(PRIORITY_QUEUE, PushToNullQueue) {
    int rc = po_priority_queue_push(NULL, &test_ints[0]);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

TEST(PRIORITY_QUEUE, PushDuplicateElement) {
    po_priority_queue_push(pq, &test_ints[5]);
    int rc = po_priority_queue_push(pq, &test_ints[5]); // Same pointer
    TEST_ASSERT_EQUAL_INT(-1, rc);                      // Should fail as duplicate
    TEST_ASSERT_EQUAL_UINT(1, po_priority_queue_size(pq));
}

// ============================================================================
// Unit Tests: Pop Operations
// ============================================================================

TEST(PRIORITY_QUEUE, PopEmpty) {
    void *result = po_priority_queue_pop(pq);
    TEST_ASSERT_NULL(result);
}

TEST(PRIORITY_QUEUE, PopFromNull) {
    void *result = po_priority_queue_pop(NULL);
    TEST_ASSERT_NULL(result);
}

TEST(PRIORITY_QUEUE, PopSingle) {
    po_priority_queue_push(pq, &test_ints[7]);
    void *result = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_PTR(&test_ints[7], result);
    TEST_ASSERT_TRUE(po_priority_queue_is_empty(pq));
}

TEST(PRIORITY_QUEUE, PopReturnsMinimum) {
    // Push in random order
    po_priority_queue_push(pq, &test_ints[5]);
    po_priority_queue_push(pq, &test_ints[3]);
    po_priority_queue_push(pq, &test_ints[7]);
    po_priority_queue_push(pq, &test_ints[1]);
    po_priority_queue_push(pq, &test_ints[9]);

    // Pop should return in ascending order
    int *v1 = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_INT(1, *v1);
    int *v2 = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_INT(3, *v2);
    int *v3 = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_INT(5, *v3);
    int *v4 = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_INT(7, *v4);
    int *v5 = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_INT(9, *v5);
    TEST_ASSERT_TRUE(po_priority_queue_is_empty(pq));
}

// ============================================================================
// Unit Tests: Peek Operations
// ============================================================================

TEST(PRIORITY_QUEUE, PeekEmpty) {
    void *result = po_priority_queue_peek(pq);
    TEST_ASSERT_NULL(result);
}

TEST(PRIORITY_QUEUE, PeekFromNull) {
    void *result = po_priority_queue_peek(NULL);
    TEST_ASSERT_NULL(result);
}

TEST(PRIORITY_QUEUE, PeekReturnsMinimumWithoutRemoving) {
    po_priority_queue_push(pq, &test_ints[10]);
    po_priority_queue_push(pq, &test_ints[2]);
    po_priority_queue_push(pq, &test_ints[8]);

    int *peeked = po_priority_queue_peek(pq);
    TEST_ASSERT_EQUAL_INT(2, *peeked);
    TEST_ASSERT_EQUAL_UINT(3, po_priority_queue_size(pq)); // Size unchanged

    // Peek again, same result
    int *peeked2 = po_priority_queue_peek(pq);
    TEST_ASSERT_EQUAL_PTR(peeked, peeked2);
}

// ============================================================================
// Unit Tests: Remove Operations
// ============================================================================

TEST(PRIORITY_QUEUE, RemoveExisting) {
    po_priority_queue_push(pq, &test_ints[1]);
    po_priority_queue_push(pq, &test_ints[5]);
    po_priority_queue_push(pq, &test_ints[3]);

    int rc = po_priority_queue_remove(pq, &test_ints[5]);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT(2, po_priority_queue_size(pq));

    // Verify heap ordering maintained
    int *min = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_INT(1, *min);
    min = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_INT(3, *min);
}

TEST(PRIORITY_QUEUE, RemoveNonExisting) {
    po_priority_queue_push(pq, &test_ints[1]);
    int rc = po_priority_queue_remove(pq, &test_ints[99]);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_UINT(1, po_priority_queue_size(pq));
}

TEST(PRIORITY_QUEUE, RemoveFromNull) {
    int rc = po_priority_queue_remove(NULL, &test_ints[0]);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

TEST(PRIORITY_QUEUE, RemoveNullElement) {
    po_priority_queue_push(pq, &test_ints[1]);
    int rc = po_priority_queue_remove(pq, NULL);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

TEST(PRIORITY_QUEUE, RemoveRoot) {
    po_priority_queue_push(pq, &test_ints[5]);
    po_priority_queue_push(pq, &test_ints[1]); // This becomes root
    po_priority_queue_push(pq, &test_ints[3]);

    int rc = po_priority_queue_remove(pq, &test_ints[1]); // Remove root
    TEST_ASSERT_EQUAL_INT(0, rc);

    int *new_min = po_priority_queue_peek(pq);
    TEST_ASSERT_EQUAL_INT(3, *new_min);
}

TEST(PRIORITY_QUEUE, RemoveLast) {
    po_priority_queue_push(pq, &test_ints[3]);
    po_priority_queue_push(pq, &test_ints[1]);
    po_priority_queue_push(pq, &test_ints[5]); // This is last in vector

    int rc = po_priority_queue_remove(pq, &test_ints[5]);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT(2, po_priority_queue_size(pq));
}

// ============================================================================
// Unit Tests: Size and IsEmpty
// ============================================================================

TEST(PRIORITY_QUEUE, SizeEmpty) {
    TEST_ASSERT_EQUAL_UINT(0, po_priority_queue_size(pq));
}

TEST(PRIORITY_QUEUE, SizeNull) {
    TEST_ASSERT_EQUAL_UINT(0, po_priority_queue_size(NULL));
}

TEST(PRIORITY_QUEUE, IsEmptyTrue) {
    TEST_ASSERT_TRUE(po_priority_queue_is_empty(pq));
}

TEST(PRIORITY_QUEUE, IsEmptyFalse) {
    po_priority_queue_push(pq, &test_ints[0]);
    TEST_ASSERT_FALSE(po_priority_queue_is_empty(pq));
}

TEST(PRIORITY_QUEUE, IsEmptyNull) {
    TEST_ASSERT_TRUE(po_priority_queue_is_empty(NULL));
}

// ============================================================================
// Unit Tests: Heap Ordering
// ============================================================================

TEST(PRIORITY_QUEUE, HeapOrderingAscending) {
    // Push ascending
    for (int i = 0; i < 20; i++) {
        po_priority_queue_push(pq, &test_ints[i]);
    }

    for (int i = 0; i < 20; i++) {
        int *v = po_priority_queue_pop(pq);
        TEST_ASSERT_EQUAL_INT(i, *v);
    }
}

TEST(PRIORITY_QUEUE, HeapOrderingDescending) {
    // Push descending
    for (int i = 19; i >= 0; i--) {
        po_priority_queue_push(pq, &test_ints[i]);
    }

    for (int i = 0; i < 20; i++) {
        int *v = po_priority_queue_pop(pq);
        TEST_ASSERT_EQUAL_INT(i, *v);
    }
}

TEST(PRIORITY_QUEUE, HeapOrderingRandom) {
    // Push in "random" order
    int order[] = {15, 3, 7, 1, 12, 0, 9, 18, 5, 11};
    for (int i = 0; i < 10; i++) {
        po_priority_queue_push(pq, &test_ints[order[i]]);
    }

    int expected[] = {0, 1, 3, 5, 7, 9, 11, 12, 15, 18};
    for (int i = 0; i < 10; i++) {
        int *v = po_priority_queue_pop(pq);
        TEST_ASSERT_EQUAL_INT(expected[i], *v);
    }
}

// ============================================================================
// Unit Tests: Max-Heap (Custom Comparator)
// ============================================================================

TEST(PRIORITY_QUEUE, MaxHeapOrdering) {
    po_priority_queue_t *max_pq = po_priority_queue_create(cmp_int_max, hash_ptr);
    TEST_ASSERT_NOT_NULL(max_pq);

    po_priority_queue_push(max_pq, &test_ints[5]);
    po_priority_queue_push(max_pq, &test_ints[3]);
    po_priority_queue_push(max_pq, &test_ints[8]);
    po_priority_queue_push(max_pq, &test_ints[1]);

    // Pop should return in descending order
    int *v = po_priority_queue_pop(max_pq);
    TEST_ASSERT_EQUAL_INT(8, *v);
    v = po_priority_queue_pop(max_pq);
    TEST_ASSERT_EQUAL_INT(5, *v);
    v = po_priority_queue_pop(max_pq);
    TEST_ASSERT_EQUAL_INT(3, *v);
    v = po_priority_queue_pop(max_pq);
    TEST_ASSERT_EQUAL_INT(1, *v);

    po_priority_queue_destroy(max_pq);
}

// ============================================================================
// Unit Tests: String Elements
// ============================================================================

TEST(PRIORITY_QUEUE, StringElements) {
    po_priority_queue_t *str_pq = po_priority_queue_create(cmp_str, hash_str);
    TEST_ASSERT_NOT_NULL(str_pq);

    char *apple = "apple";
    char *banana = "banana";
    char *cherry = "cherry";
    char *avocado = "avocado";

    po_priority_queue_push(str_pq, cherry);
    po_priority_queue_push(str_pq, apple);
    po_priority_queue_push(str_pq, banana);
    po_priority_queue_push(str_pq, avocado);

    TEST_ASSERT_EQUAL_STRING("apple", (char *)po_priority_queue_pop(str_pq));
    TEST_ASSERT_EQUAL_STRING("avocado", (char *)po_priority_queue_pop(str_pq));
    TEST_ASSERT_EQUAL_STRING("banana", (char *)po_priority_queue_pop(str_pq));
    TEST_ASSERT_EQUAL_STRING("cherry", (char *)po_priority_queue_pop(str_pq));

    po_priority_queue_destroy(str_pq);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST(PRIORITY_QUEUE, StressPushPop) {
    // Push 1000 elements
    for (int i = 0; i < 1000; i++) {
        int rc = po_priority_queue_push(pq, &test_ints[i]);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }
    TEST_ASSERT_EQUAL_UINT(1000, po_priority_queue_size(pq));

    // Pop all, verify ordering
    for (int i = 0; i < 1000; i++) {
        int *v = po_priority_queue_pop(pq);
        TEST_ASSERT_NOT_NULL(v);
        TEST_ASSERT_EQUAL_INT(i, *v);
    }
    TEST_ASSERT_TRUE(po_priority_queue_is_empty(pq));
}

TEST(PRIORITY_QUEUE, StressInterleavedPushPop) {
    // Push 10, pop 5, push 10, pop 5, ...
    int push_count = 0;
    int pop_count = 0;

    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 10 && push_count < 1000; i++) {
            po_priority_queue_push(pq, &test_ints[push_count++]);
        }
        for (int i = 0; i < 5 && !po_priority_queue_is_empty(pq); i++) {
            int *v = po_priority_queue_pop(pq);
            TEST_ASSERT_NOT_NULL(v);
            TEST_ASSERT_EQUAL_INT(pop_count++, *v);
        }
    }

    // Drain remaining
    while (!po_priority_queue_is_empty(pq)) {
        int *v = po_priority_queue_pop(pq);
        TEST_ASSERT_EQUAL_INT(pop_count++, *v);
    }

    TEST_ASSERT_EQUAL_INT(push_count, pop_count);
}

TEST(PRIORITY_QUEUE, StressRandomRemoval) {
    // Push 50 elements
    for (int i = 0; i < 50; i++) {
        po_priority_queue_push(pq, &test_ints[i]);
    }

    // Remove every 5th element
    for (int i = 4; i < 50; i += 5) {
        int rc = po_priority_queue_remove(pq, &test_ints[i]);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }

    TEST_ASSERT_EQUAL_UINT(40, po_priority_queue_size(pq));

    // Verify remaining elements in order
    int last = -1;
    while (!po_priority_queue_is_empty(pq)) {
        int *v = po_priority_queue_pop(pq);
        TEST_ASSERT_TRUE(*v > last);
        TEST_ASSERT_TRUE(*v % 5 != 4); // Removed elements
        last = *v;
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(PRIORITY_QUEUE, IntegrationMixedOperations) {
    // Complex sequence of operations
    po_priority_queue_push(pq, &test_ints[50]);
    po_priority_queue_push(pq, &test_ints[30]);
    po_priority_queue_push(pq, &test_ints[70]);
    po_priority_queue_push(pq, &test_ints[10]);
    po_priority_queue_push(pq, &test_ints[90]);

    TEST_ASSERT_EQUAL_INT(10, *(int *)po_priority_queue_peek(pq));

    // Remove middle element
    po_priority_queue_remove(pq, &test_ints[50]);

    // Pop two
    int *v1 = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_INT(10, *v1);
    int *v2 = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_INT(30, *v2);

    // Push new minimum
    po_priority_queue_push(pq, &test_ints[5]);
    TEST_ASSERT_EQUAL_INT(5, *(int *)po_priority_queue_peek(pq));

    // Drain
    TEST_ASSERT_EQUAL_INT(5, *(int *)po_priority_queue_pop(pq));
    TEST_ASSERT_EQUAL_INT(70, *(int *)po_priority_queue_pop(pq));
    TEST_ASSERT_EQUAL_INT(90, *(int *)po_priority_queue_pop(pq));
    TEST_ASSERT_TRUE(po_priority_queue_is_empty(pq));
}

TEST(PRIORITY_QUEUE, IntegrationRepeatedPushPopSameElement) {
    // Push, pop, push same element again
    po_priority_queue_push(pq, &test_ints[42]);
    int *v = po_priority_queue_pop(pq);
    TEST_ASSERT_EQUAL_INT(42, *v);

    // Push again (should succeed since it was removed)
    int rc = po_priority_queue_push(pq, &test_ints[42]);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT(1, po_priority_queue_size(pq));
}

// ============================================================================
// Test Group Runner
// ============================================================================

TEST_GROUP_RUNNER(PRIORITY_QUEUE) {
    // Creation and Destruction
    RUN_TEST_CASE(PRIORITY_QUEUE, CreateAndDestroy);
    RUN_TEST_CASE(PRIORITY_QUEUE, CreateWithNullComparator);
    RUN_TEST_CASE(PRIORITY_QUEUE, CreateWithNullHash);
    RUN_TEST_CASE(PRIORITY_QUEUE, DestroyNull);

    // Push Operations
    RUN_TEST_CASE(PRIORITY_QUEUE, PushSingle);
    RUN_TEST_CASE(PRIORITY_QUEUE, PushMultiple);
    RUN_TEST_CASE(PRIORITY_QUEUE, PushNullElement);
    RUN_TEST_CASE(PRIORITY_QUEUE, PushToNullQueue);
    RUN_TEST_CASE(PRIORITY_QUEUE, PushDuplicateElement);

    // Pop Operations
    RUN_TEST_CASE(PRIORITY_QUEUE, PopEmpty);
    RUN_TEST_CASE(PRIORITY_QUEUE, PopFromNull);
    RUN_TEST_CASE(PRIORITY_QUEUE, PopSingle);
    RUN_TEST_CASE(PRIORITY_QUEUE, PopReturnsMinimum);

    // Peek Operations
    RUN_TEST_CASE(PRIORITY_QUEUE, PeekEmpty);
    RUN_TEST_CASE(PRIORITY_QUEUE, PeekFromNull);
    RUN_TEST_CASE(PRIORITY_QUEUE, PeekReturnsMinimumWithoutRemoving);

    // Remove Operations
    RUN_TEST_CASE(PRIORITY_QUEUE, RemoveExisting);
    RUN_TEST_CASE(PRIORITY_QUEUE, RemoveNonExisting);
    RUN_TEST_CASE(PRIORITY_QUEUE, RemoveFromNull);
    RUN_TEST_CASE(PRIORITY_QUEUE, RemoveNullElement);
    RUN_TEST_CASE(PRIORITY_QUEUE, RemoveRoot);
    RUN_TEST_CASE(PRIORITY_QUEUE, RemoveLast);

    // Size and IsEmpty
    RUN_TEST_CASE(PRIORITY_QUEUE, SizeEmpty);
    RUN_TEST_CASE(PRIORITY_QUEUE, SizeNull);
    RUN_TEST_CASE(PRIORITY_QUEUE, IsEmptyTrue);
    RUN_TEST_CASE(PRIORITY_QUEUE, IsEmptyFalse);
    RUN_TEST_CASE(PRIORITY_QUEUE, IsEmptyNull);

    // Heap Ordering
    RUN_TEST_CASE(PRIORITY_QUEUE, HeapOrderingAscending);
    RUN_TEST_CASE(PRIORITY_QUEUE, HeapOrderingDescending);
    RUN_TEST_CASE(PRIORITY_QUEUE, HeapOrderingRandom);

    // Max-Heap
    RUN_TEST_CASE(PRIORITY_QUEUE, MaxHeapOrdering);

    // String Elements
    RUN_TEST_CASE(PRIORITY_QUEUE, StringElements);

    // Stress Tests
    RUN_TEST_CASE(PRIORITY_QUEUE, StressPushPop);
    RUN_TEST_CASE(PRIORITY_QUEUE, StressInterleavedPushPop);
    RUN_TEST_CASE(PRIORITY_QUEUE, StressRandomRemoval);

    // Integration Tests
    RUN_TEST_CASE(PRIORITY_QUEUE, IntegrationMixedOperations);
    RUN_TEST_CASE(PRIORITY_QUEUE, IntegrationRepeatedPushPopSameElement);
}
