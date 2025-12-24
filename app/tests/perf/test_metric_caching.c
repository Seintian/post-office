#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "metrics/metrics.h"
#include "perf/perf.h"
#include "unity/unity_fixture.h"

// Test configuration
#define NUM_THREADS 20
#define INCREMENTS_PER_THREAD 10000
#define NUM_UNIQUE_METRICS 50

// Thread barrier for synchronized starts
static pthread_barrier_t g_barrier;

// Atomic counter for verification
static atomic_uint_fast64_t g_expected_total;

TEST_GROUP(METRIC_CACHING);

TEST_SETUP(METRIC_CACHING) {
    // Clean shutdown any previous instance
    po_perf_shutdown(NULL);
    
    // Initialize with sufficient capacity
    TEST_ASSERT_EQUAL_INT(0, po_perf_init(128, 32, 16));
    TEST_ASSERT_EQUAL_INT(0, po_metrics_init(0, 0, 0));
    
    atomic_store(&g_expected_total, 0);
}

TEST_TEAR_DOWN(METRIC_CACHING) {
    po_metrics_shutdown();
    po_perf_shutdown(NULL);
}

// Helper to get counter value
static uint64_t get_counter_value(const char *name) {
    FILE *tmp = tmpfile();
    if (!tmp) return 0;
    
    po_perf_report(tmp);
    fflush(tmp);
    rewind(tmp);
    
    char line[256];
    uint64_t value = 0;
    size_t name_len = strlen(name);
    
    while (fgets(line, sizeof(line), tmp)) {
        // Skip leading whitespace
        char *start = line;
        while (*start == ' ' || *start == '\t') start++;
        
        // Check if line starts with metric name followed by ':'
        if (strncmp(start, name, name_len) == 0 && start[name_len] == ':') {
            value = strtoull(start + name_len + 1, NULL, 10);
            break;
        }
    }
    
    fclose(tmp);
    return value;
}

// Test 1: Basic cache correctness - single thread
TEST(METRIC_CACHING, SINGLE_THREAD_CACHE_CORRECTNESS) {
    const char *counter_name = "cache_test_counter";
    
    // First call should cache the index
    PO_METRIC_COUNTER_INC(counter_name);
    TEST_ASSERT_EQUAL_UINT64(1, get_counter_value(counter_name));
    
    // Subsequent calls should use cached index
    for (int i = 0; i < 1000; i++) {
        PO_METRIC_COUNTER_INC(counter_name);
    }
    
    TEST_ASSERT_EQUAL_UINT64(1001, get_counter_value(counter_name));
}

// Test 2: Multiple metrics in same thread
TEST(METRIC_CACHING, MULTIPLE_METRICS_SAME_THREAD) {
    for (int i = 0; i < 10; i++) {
        char name[64];
        snprintf(name, sizeof(name), "test2_multi_%d", i);
        
        for (int j = 0; j < 100; j++) {
            PO_METRIC_COUNTER_INC(name);
        }
        
        TEST_ASSERT_EQUAL_UINT64(100, get_counter_value(name));
    }
}

// Test 3: Counter ADD with caching
TEST(METRIC_CACHING, COUNTER_ADD_CACHING) {
    const char *counter_name = "add_test";
    
    PO_METRIC_COUNTER_ADD(counter_name, 5);
    PO_METRIC_COUNTER_ADD(counter_name, 10);
    PO_METRIC_COUNTER_ADD(counter_name, 15);
    
    TEST_ASSERT_EQUAL_UINT64(30, get_counter_value(counter_name));
}

// Test 4: Timer caching
TEST(METRIC_CACHING, TIMER_CACHING) {
    const char *timer_name = "timer_test";
    
    PO_METRIC_TIMER_CREATE(timer_name);
    
    for (int i = 0; i < 10; i++) {
        PO_METRIC_TIMER_START(timer_name);
        usleep(100); // 100 microseconds
        PO_METRIC_TIMER_STOP(timer_name);
    }
    
    // Verify timer has accumulated time
    FILE *tmp = tmpfile();
    po_perf_report(tmp);
    fflush(tmp);
    rewind(tmp);
    
    char buf[4096];
    size_t len = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[len] = '\0';
    fclose(tmp);
    
    TEST_ASSERT_NOT_NULL(strstr(buf, timer_name));
}

// Test 5: Histogram caching
TEST(METRIC_CACHING, HISTOGRAM_CACHING) {
    const char *hist_name = "hist_test";
    uint64_t bins[] = {10, 100, 1000};
    
    PO_METRIC_HISTO_CREATE(hist_name, bins, 3);
    
    for (int i = 0; i < 100; i++) {
        PO_METRIC_HISTO_RECORD(hist_name, (uint64_t)(i % 150));
    }
    
    FILE *tmp = tmpfile();
    po_perf_report(tmp);
    fflush(tmp);
    rewind(tmp);
    
    char buf[4096];
    size_t len = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[len] = '\0';
    fclose(tmp);
    
    TEST_ASSERT_NOT_NULL(strstr(buf, hist_name));
}

// Thread function for multi-threaded cache test
static void *thread_cache_test(void *arg) {
    const char *counter_name = (const char *)arg;
    
    pthread_barrier_wait(&g_barrier);
    
    // Each thread increments the same counter many times
    // Cache should work independently per thread
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        PO_METRIC_COUNTER_INC(counter_name);
        atomic_fetch_add(&g_expected_total, 1);
    }
    
    return NULL;
}

// Test 6: Multi-threaded cache correctness
TEST(METRIC_CACHING, MULTI_THREADED_CACHE_CORRECTNESS) {
    const char *counter_name = "mt_cache_counter";
    
    pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);
    
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        TEST_ASSERT_EQUAL_INT(0, pthread_create(&threads[i], NULL, 
                                                 thread_cache_test, 
                                                 (void *)counter_name));
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&g_barrier);
    
    uint64_t expected = atomic_load(&g_expected_total);
    uint64_t actual = get_counter_value(counter_name);
    
    TEST_ASSERT_EQUAL_UINT64(expected, actual);
}

// Thread function for unique metrics per thread
static void *thread_unique_metrics(void *arg) {
    int thread_id = *(int *)arg;
    
    pthread_barrier_wait(&g_barrier);
    
    // Each thread has its own metric
    char name[64];
    snprintf(name, sizeof(name), "thread_%d_metric", thread_id);
    
    for (int i = 0; i < 1000; i++) {
        PO_METRIC_COUNTER_INC(name);
    }
    
    return NULL;
}

// Test 7: TLS isolation - each thread caches independently
TEST(METRIC_CACHING, TLS_ISOLATION) {
    pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);
    
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_unique_metrics, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&g_barrier);
    
    // Verify each thread's metric
    for (int i = 0; i < NUM_THREADS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "thread_%d_metric", i);
        TEST_ASSERT_EQUAL_UINT64(1000, get_counter_value(name));
    }
}

// Thread function for mixed operations
static void *thread_mixed_ops(void *arg) {
    int thread_id = *(int *)arg;
    
    pthread_barrier_wait(&g_barrier);
    
    char counter_name[64];
    char timer_name[64];
    snprintf(counter_name, sizeof(counter_name), "mixed_counter_%d", thread_id);
    snprintf(timer_name, sizeof(timer_name), "mixed_timer_%d", thread_id);
    
    PO_METRIC_TIMER_CREATE(timer_name);
    
    for (int i = 0; i < 100; i++) {
        PO_METRIC_COUNTER_INC(counter_name);
        PO_METRIC_COUNTER_ADD(counter_name, 2);
        
        PO_METRIC_TIMER_START(timer_name);
        // Simulate work
        for (volatile int j = 0; j < 100; j++);
        PO_METRIC_TIMER_STOP(timer_name);
    }
    
    return NULL;
}

// Test 8: Mixed operations with caching
TEST(METRIC_CACHING, MIXED_OPERATIONS_MULTI_THREADED) {
    pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);
    
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_mixed_ops, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&g_barrier);
    
    // Verify counters (100 INC + 100 ADD(2) = 300)
    for (int i = 0; i < NUM_THREADS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "mixed_counter_%d", i);
        TEST_ASSERT_EQUAL_UINT64(300, get_counter_value(name));
    }
}

// Test 9: Cache performance - verify caching reduces overhead
TEST(METRIC_CACHING, CACHE_PERFORMANCE_BENEFIT) {
    const char *counter_name = "perf_test";
    
    struct timespec start, end;
    
    // Warm up cache
    PO_METRIC_COUNTER_INC(counter_name);
    
    // Measure cached performance
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < 100000; i++) {
        PO_METRIC_COUNTER_INC(counter_name);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    uint64_t elapsed_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (uint64_t)(end.tv_nsec - start.tv_nsec);
    
    // Should complete in reasonable time (< 50ms for 100k increments)
    TEST_ASSERT_LESS_THAN_UINT64(50000000, elapsed_ns);
    
    // Verify correctness
    TEST_ASSERT_EQUAL_UINT64(100001, get_counter_value(counter_name));
}

// Thread function for stress test
static void *stress_thread_func(void *arg) {
    (void)arg; // Unused
    
    for (int i = 0; i < NUM_UNIQUE_METRICS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "test10_stress_%d", i);
        
        for (int j = 0; j < 10; j++) {
            PO_METRIC_COUNTER_INC(name);
        }
    }
    
    return NULL;
}

// Test 10: Stress test - many metrics, many threads
TEST(METRIC_CACHING, STRESS_TEST_MANY_METRICS) {
    #define STRESS_NUM_THREADS 10
    
    // Pre-create metrics
    for (int i = 0; i < NUM_UNIQUE_METRICS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "test10_stress_%d", i);
        PO_METRIC_COUNTER_CREATE(name);
    }
    
    pthread_t threads[STRESS_NUM_THREADS];
    
    for (int i = 0; i < STRESS_NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, stress_thread_func, NULL);
    }
    
    for (int i = 0; i < STRESS_NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Verify each metric (10 threads × 10 increments = 100)
    for (int i = 0; i < NUM_UNIQUE_METRICS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "test10_stress_%d", i);
        TEST_ASSERT_EQUAL_UINT64(100, get_counter_value(name));
    }
    
    #undef STRESS_NUM_THREADS
}

// Test 11: Cache with dynamic metric creation
TEST(METRIC_CACHING, DYNAMIC_METRIC_CREATION) {
    // Metrics created on first use (no explicit CREATE call)
    for (int i = 0; i < 20; i++) {
        char name[64];
        snprintf(name, sizeof(name), "test11_dyn_%d", i);
        
        // First use creates and caches
        PO_METRIC_COUNTER_INC(name);
        PO_METRIC_COUNTER_INC(name);
        
        TEST_ASSERT_EQUAL_UINT64(2, get_counter_value(name));
    }
}

TEST_GROUP_RUNNER(METRIC_CACHING) {
    RUN_TEST_CASE(METRIC_CACHING, SINGLE_THREAD_CACHE_CORRECTNESS);
    RUN_TEST_CASE(METRIC_CACHING, MULTIPLE_METRICS_SAME_THREAD);
    RUN_TEST_CASE(METRIC_CACHING, COUNTER_ADD_CACHING);
    RUN_TEST_CASE(METRIC_CACHING, TIMER_CACHING);
    RUN_TEST_CASE(METRIC_CACHING, HISTOGRAM_CACHING);
    RUN_TEST_CASE(METRIC_CACHING, MULTI_THREADED_CACHE_CORRECTNESS);
    RUN_TEST_CASE(METRIC_CACHING, TLS_ISOLATION);
    RUN_TEST_CASE(METRIC_CACHING, MIXED_OPERATIONS_MULTI_THREADED);
    RUN_TEST_CASE(METRIC_CACHING, CACHE_PERFORMANCE_BENEFIT);
    RUN_TEST_CASE(METRIC_CACHING, STRESS_TEST_MANY_METRICS);
    RUN_TEST_CASE(METRIC_CACHING, DYNAMIC_METRIC_CREATION);
}
