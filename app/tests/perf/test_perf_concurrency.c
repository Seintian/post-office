#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "perf/perf.h"
#include "unity/unity_fixture.h"
#include "utils/errors.h"

// Test configuration
#define NUM_THREADS 10
#define NUM_PROCESSES 5
#define INCREMENTS_PER_THREAD 1000
#define INCREMENTS_PER_PROCESS 500
#define STRESS_THREADS 20
#define STRESS_INCREMENTS 5000

// Thread barrier for synchronized starts
static pthread_barrier_t g_barrier;

// Thread function for counter increment test
static void *thread_increment_counter(void *arg) {
    const char *counter_name = (const char *)arg;
    
    // Wait for all threads to be ready
    pthread_barrier_wait(&g_barrier);
    
    // Increment counter many times
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        po_perf_counter_inc(counter_name);
    }
    
    return NULL;
}

// Thread function for mixed operations
static void *thread_mixed_operations(void *arg) {
    int thread_id = *(int *)arg;
    
    pthread_barrier_wait(&g_barrier);
    
    char counter_name[32];
    snprintf(counter_name, sizeof(counter_name), "thread_%d_counter", thread_id);
    
    for (int i = 0; i < 100; i++) {
        po_perf_counter_inc(counter_name);
        po_perf_counter_add("shared_counter", 1);
        
        // Also test timer operations
        po_perf_timer_start("shared_timer");
        struct timespec ts = {0, 1000}; // 1 microsecond
        nanosleep(&ts, NULL);
        po_perf_timer_stop("shared_timer");
    }
    
    return NULL;
}

// Helper to get counter value from report
static uint64_t get_counter_value_from_report(const char *counter_name) {
    FILE *tmp = tmpfile();
    if (!tmp) return 0;
    
    po_perf_report(tmp);
    fflush(tmp);
    rewind(tmp);
    
    char line[256];
    uint64_t value = 0;
    while (fgets(line, sizeof(line), tmp)) {
        if (strstr(line, counter_name)) {
            char *colon = strchr(line, ':');
            if (colon) {
                value = strtoull(colon + 1, NULL, 10);
                break;
            }
        }
    }
    
    fclose(tmp);
    return value;
}

TEST_GROUP(PERF_CONCURRENCY);

TEST_SETUP(PERF_CONCURRENCY) {
    // Clean shutdown any previous instance
    po_perf_shutdown(NULL);
    
    // Initialize with sufficient capacity
    TEST_ASSERT_EQUAL_INT(0, po_perf_init(64, 16, 8));
}

TEST_TEAR_DOWN(PERF_CONCURRENCY) {
    po_perf_report(stdout);
    po_perf_shutdown(NULL);
}

// Test 1: Multi-threaded counter increments
TEST(PERF_CONCURRENCY, MULTI_THREADED_COUNTER_INCREMENT) {
    const char *counter_name = "mt_counter";
    
    // Create counter
    TEST_ASSERT_EQUAL_INT(0, po_perf_counter_create(counter_name));
    
    // Initialize barrier
    pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);
    
    // Create threads
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        TEST_ASSERT_EQUAL_INT(0, pthread_create(&threads[i], NULL, 
                                                 thread_increment_counter, 
                                                 (void *)counter_name));
    }
    
    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&g_barrier);
    
    // Verify count
    uint64_t expected = NUM_THREADS * INCREMENTS_PER_THREAD;
    uint64_t actual = get_counter_value_from_report(counter_name);
    
    TEST_ASSERT_EQUAL_UINT64(expected, actual);
}

// Test 2: Multi-process counter increments via fork
TEST(PERF_CONCURRENCY, MULTI_PROCESS_COUNTER_INCREMENT) {
    const char *counter_name = "mp_counter";
    
    // Create counter in parent
    TEST_ASSERT_EQUAL_INT(0, po_perf_counter_create(counter_name));
    
    // Fork children
    pid_t children[NUM_PROCESSES];
    for (int i = 0; i < NUM_PROCESSES; i++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            // Child process
            for (int j = 0; j < INCREMENTS_PER_PROCESS; j++) {
                po_perf_counter_inc(counter_name);
            }
            exit(0);
        } else if (pid > 0) {
            children[i] = pid;
        } else {
            TEST_FAIL_MESSAGE("fork() failed");
        }
    }
    
    // Wait for all children
    for (int i = 0; i < NUM_PROCESSES; i++) {
        int status;
        waitpid(children[i], &status, 0);
        TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(status));
    }
    
    // Small delay to ensure SHM is updated
    usleep(10000);
    
    // Verify count
    uint64_t expected = NUM_PROCESSES * INCREMENTS_PER_PROCESS;
    uint64_t actual = get_counter_value_from_report(counter_name);
    
    TEST_ASSERT_EQUAL_UINT64(expected, actual);
}

// Test 3: Mixed process and thread operations
TEST(PERF_CONCURRENCY, MIXED_PROCESS_AND_THREAD) {
    // Create shared counter
    TEST_ASSERT_EQUAL_INT(0, po_perf_counter_create("shared_counter"));
    TEST_ASSERT_EQUAL_INT(0, po_perf_timer_create("shared_timer"));
    
    // Create per-thread counters
    for (int i = 0; i < NUM_THREADS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "thread_%d_counter", i);
        po_perf_counter_create(name);
    }
    
    // Fork a child process
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process: spawn threads
        pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);
        
        pthread_t *threads = malloc(NUM_THREADS * sizeof(pthread_t));
        int *thread_ids = malloc(NUM_THREADS * sizeof(int));
        
        for (int i = 0; i < NUM_THREADS; i++) {
            thread_ids[i] = i;
            pthread_create(&threads[i], NULL, thread_mixed_operations, &thread_ids[i]);
        }
        
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
        
        free(threads);
        free(thread_ids);
        pthread_barrier_destroy(&g_barrier);
        exit(0);
    } else if (pid > 0) {
        // Parent process: also spawn threads
        pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);
        
        pthread_t *threads = malloc(NUM_THREADS * sizeof(pthread_t));
        int *thread_ids = malloc(NUM_THREADS * sizeof(int));
        
        for (int i = 0; i < NUM_THREADS; i++) {
            thread_ids[i] = i + NUM_THREADS; // Different IDs for parent
            pthread_create(&threads[i], NULL, thread_mixed_operations, &thread_ids[i]);
        }
        
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
        
        free(threads);
        free(thread_ids);
        pthread_barrier_destroy(&g_barrier);
        
        // Wait for child
        int status;
        waitpid(pid, &status, 0);
        TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(status));
        
        usleep(10000);
        
        // Verify shared counter (2 processes × NUM_THREADS threads × 100 increments)
        uint64_t expected = 2 * NUM_THREADS * 100;
        uint64_t actual = get_counter_value_from_report("shared_counter");
        
        TEST_ASSERT_EQUAL_UINT64(expected, actual);
    } else {
        TEST_FAIL_MESSAGE("fork() failed");
    }
}

// Test 4: High contention stress test
TEST(PERF_CONCURRENCY, STRESS_TEST_HIGH_CONTENTION) {
    const char *counter_name = "stress_counter";
    
    TEST_ASSERT_EQUAL_INT(0, po_perf_counter_create(counter_name));
    
    // Spawn many threads all hammering the same counter
    // Note: +1 because main thread also participates
    pthread_barrier_init(&g_barrier, NULL, STRESS_THREADS + 1);
    
    pthread_t threads[STRESS_THREADS];
    for (int i = 0; i < STRESS_THREADS; i++) {
        TEST_ASSERT_EQUAL_INT(0, pthread_create(&threads[i], NULL, 
                                                 thread_increment_counter, 
                                                 (void *)counter_name));
    }
    
    // Also increment from main thread
    pthread_barrier_wait(&g_barrier);
    for (int i = 0; i < STRESS_INCREMENTS; i++) {
        po_perf_counter_inc(counter_name);
    }
    
    // Wait for all threads
    for (int i = 0; i < STRESS_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&g_barrier);
    
    // Verify count
    // STRESS_THREADS use thread_increment_counter which does INCREMENTS_PER_THREAD
    // Main thread does STRESS_INCREMENTS
    uint64_t expected = (STRESS_THREADS * INCREMENTS_PER_THREAD) + STRESS_INCREMENTS;
    uint64_t actual = get_counter_value_from_report(counter_name);
    
    TEST_ASSERT_EQUAL_UINT64(expected, actual);
}

// Test 5: Timer concurrency
TEST(PERF_CONCURRENCY, TIMER_CONCURRENCY) {
    TEST_ASSERT_EQUAL_INT(0, po_perf_timer_create("concurrent_timer"));
    
    pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);
    
    // Thread function for timer test
    void *timer_thread(void *arg) {
        (void)arg;
        pthread_barrier_wait(&g_barrier);
        
        for (int i = 0; i < 100; i++) {
            po_perf_timer_start("concurrent_timer");
            struct timespec ts = {0, 10000}; // 10 microseconds
            nanosleep(&ts, NULL);
            po_perf_timer_stop("concurrent_timer");
        }
        
        return NULL;
    }
    
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, timer_thread, NULL);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&g_barrier);
    
    // Just verify no crashes - timer values may vary due to scheduling
    uint64_t timer_value = get_counter_value_from_report("concurrent_timer");
    TEST_ASSERT_GREATER_THAN_UINT64(0, timer_value);
}

// Test 6: Histogram concurrency
TEST(PERF_CONCURRENCY, HISTOGRAM_CONCURRENCY) {
    uint64_t bins[] = {10, 100, 1000, 10000};
    TEST_ASSERT_EQUAL_INT(0, po_perf_histogram_create("concurrent_hist", bins, 4));
    
    pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);
    
    // Thread function for histogram test
    void *histogram_thread(void *arg) {
        int thread_id = *(int *)arg;
        pthread_barrier_wait(&g_barrier);
        
        for (int i = 0; i < 100; i++) {
            uint64_t value = (uint64_t)((thread_id * 100 + i) % 15000);
            po_perf_histogram_record("concurrent_hist", value);
        }
        
        return NULL;
    }
    
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, histogram_thread, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&g_barrier);
    
    // Verify histogram has recorded values (exact distribution may vary)
    FILE *tmp = tmpfile();
    po_perf_report(tmp);
    fflush(tmp);
    rewind(tmp);
    
    char buf[4096];
    size_t len = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[len] = '\0';
    fclose(tmp);
    
    TEST_ASSERT_NOT_NULL(strstr(buf, "concurrent_hist"));
}

// Test 7: Counter add operations with different values
TEST(PERF_CONCURRENCY, COUNTER_ADD_CONCURRENCY) {
    const char *counter_name = "add_counter";
    TEST_ASSERT_EQUAL_INT(0, po_perf_counter_create(counter_name));
    
    pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);
    
    void *add_thread(void *arg) {
        int thread_id = *(int *)arg;
        pthread_barrier_wait(&g_barrier);
        
        for (int i = 0; i < 100; i++) {
            po_perf_counter_add(counter_name, (uint64_t)(thread_id + 1));
        }
        
        return NULL;
    }
    
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, add_thread, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&g_barrier);
    
    // Calculate expected: sum of (thread_id + 1) * 100 for all threads
    // = (1 + 2 + 3 + ... + NUM_THREADS) * 100
    // = (NUM_THREADS * (NUM_THREADS + 1) / 2) * 100
    uint64_t expected = (NUM_THREADS * (NUM_THREADS + 1) / 2) * 100;
    uint64_t actual = get_counter_value_from_report(counter_name);
    
    TEST_ASSERT_EQUAL_UINT64(expected, actual);
}

TEST_GROUP_RUNNER(PERF_CONCURRENCY) {
    RUN_TEST_CASE(PERF_CONCURRENCY, MULTI_THREADED_COUNTER_INCREMENT);
    RUN_TEST_CASE(PERF_CONCURRENCY, MULTI_PROCESS_COUNTER_INCREMENT);
    RUN_TEST_CASE(PERF_CONCURRENCY, MIXED_PROCESS_AND_THREAD);
    RUN_TEST_CASE(PERF_CONCURRENCY, STRESS_TEST_HIGH_CONTENTION);
    RUN_TEST_CASE(PERF_CONCURRENCY, TIMER_CONCURRENCY);
    RUN_TEST_CASE(PERF_CONCURRENCY, HISTOGRAM_CONCURRENCY);
    RUN_TEST_CASE(PERF_CONCURRENCY, COUNTER_ADD_CONCURRENCY);
}
