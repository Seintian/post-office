#include "unity/unity_fixture.h"
#include "postoffice/sort/sort.h"
#include "postoffice/random/random.h"
#include <stdlib.h>
#include <time.h>

TEST_GROUP(SORT);

TEST_SETUP(SORT) {
    po_rand_seed_auto(); // Ensure randomness for random tests
}
TEST_TEAR_DOWN(SORT) {}

static int int_compar(const void *a, const void *b) {
    const int *ia = (const int *)a;
    const int *ib = (const int *)b;
    return (*ia > *ib) - (*ia < *ib);
}

static int int_compar_r(const void *a, const void *b, void *arg) {
    const int *ia = (const int *)a;
    const int *ib = (const int *)b;
    int multiplier = *(int*)arg;
    return multiplier * ((*ia > *ib) - (*ia < *ib));
}

TEST(SORT, INTEGERS_DESCENDING) {
    int arr[] = { 5, 4, 3, 2, 1 };
    size_t n = 5;
    po_sort(arr, n, sizeof(int), int_compar);
    
    int expected[] = { 1, 2, 3, 4, 5 };
    TEST_ASSERT_EQUAL_INT_ARRAY(expected, arr, n);
}

TEST(SORT, INTEGERS_RANDOM) {
    int arr[] = { 3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5 };
    size_t n = 11;
    po_sort(arr, n, sizeof(int), int_compar);
    
    int expected[] = { 1, 1, 2, 3, 3, 4, 5, 5, 5, 6, 9 };
    TEST_ASSERT_EQUAL_INT_ARRAY(expected, arr, n);
}

TEST(SORT, INTEGERS_RANDOM_LARGE) {
    size_t n = 1000;
    int *arr = malloc(n * sizeof(int));
    for(size_t i=0; i<n; i++) arr[i] = (int)po_rand_u32();
    
    po_sort(arr, n, sizeof(int), int_compar);
    
    for(size_t i=1; i<n; i++) {
        TEST_ASSERT_TRUE(arr[i-1] <= arr[i]);
    }
    free(arr);
}

TEST(SORT, INTEGERS_FEW_UNIQUE_LARGE) {
    size_t n = 10000;
    int *arr = malloc(n * sizeof(int));
    for(size_t i=0; i<n; i++) arr[i] = (int)po_rand_range_i64(0, 9);
    
    po_sort(arr, n, sizeof(int), int_compar);
    
    for(size_t i=1; i<n; i++) {
        TEST_ASSERT_TRUE(arr[i-1] <= arr[i]);
    }
    free(arr);
}

TEST(SORT, SORTR) {
    int arr[] = { 5, 4, 3, 2, 1 };
    size_t n = 5;
    int multiplier = 1; 
    po_sort_r(arr, n, sizeof(int), int_compar_r, &multiplier);
    
    int expected[] = { 1, 2, 3, 4, 5 };
    TEST_ASSERT_EQUAL_INT_ARRAY(expected, arr, n);
}

TEST(SORT, SORTR_REVERSE) {
    int arr[] = { 1, 2, 3, 4, 5 };
    size_t n = 5;
    int multiplier = -1; // Reverse sort logic
    po_sort_r(arr, n, sizeof(int), int_compar_r, &multiplier);
    
    int expected[] = { 5, 4, 3, 2, 1 };
    TEST_ASSERT_EQUAL_INT_ARRAY(expected, arr, n);
}

TEST_GROUP_RUNNER(SORT) {
    RUN_TEST_CASE(SORT, INTEGERS_DESCENDING);
    RUN_TEST_CASE(SORT, INTEGERS_RANDOM);
    RUN_TEST_CASE(SORT, INTEGERS_RANDOM_LARGE);
    RUN_TEST_CASE(SORT, INTEGERS_FEW_UNIQUE_LARGE);
    RUN_TEST_CASE(SORT, SORTR);
    RUN_TEST_CASE(SORT, SORTR_REVERSE);
}
