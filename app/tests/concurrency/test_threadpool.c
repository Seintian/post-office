#include "unity/unity_fixture.h"
#include "postoffice/concurrency/threadpool.h"
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

TEST_GROUP(THREADPOOL);

static threadpool_t *pool;
static atomic_int counter;

static void increment_task(void *arg) {
    (void)arg;
    atomic_fetch_add(&counter, 1);
}

static void slow_task(void *arg) {
    (void)arg;
    usleep(100000); // 100ms
    atomic_fetch_add(&counter, 1);
}

TEST_SETUP(THREADPOOL) {
    pool = NULL;
    atomic_store(&counter, 0);
}

TEST_TEAR_DOWN(THREADPOOL) {
    if (pool) {
        tp_destroy(pool, false);
        pool = NULL;
    }
}

TEST(THREADPOOL, CreateDestroy) {
    pool = tp_create(4, 10);
    TEST_ASSERT_NOT_NULL(pool);
    // clean up in tear down
}

TEST(THREADPOOL, InvalidCreate) {
    threadpool_t *p = tp_create(0, 10);
    TEST_ASSERT_NULL(p);
}

TEST(THREADPOOL, SubmitAndExecute) {
    pool = tp_create(2, 10);
    TEST_ASSERT_NOT_NULL(pool);

    TEST_ASSERT_EQUAL_INT(0, tp_submit(pool, increment_task, NULL));
    TEST_ASSERT_EQUAL_INT(0, tp_submit(pool, increment_task, NULL));
    
    // Give some time for execution or use graceful shutdown to wait
    tp_destroy(pool, true);
    pool = NULL;

    TEST_ASSERT_EQUAL_INT(2, atomic_load(&counter));
}

TEST(THREADPOOL, QueueFull) {
    pool = tp_create(1, 1); // 1 thread, 1 queue slot is misleading. 
    // tp_create(num_threads, queue_size).
    // queue_size relates to pending tasks.
    // If we have 1 thread, it picks one task immediately.
    // So if we submit fast:
    // 1. Thread picks task 1.
    // 2. Queue can hold 1 task (task 2).
    // 3. Task 3 should fail?
    
    // Let's try with a blocking task to ensure queue fills up.
    // However, threadpool logic: if (queue_count >= queue_size) return -1.
    
    // To reliably test queue full, we need to ensure tasks accumulate.
    // We can't pause the worker easily without internal access or slow tasks.
    
    // We need to ensure the worker thread has picked up the first task
    // before properly testing the queue capacity.
    // However, since we cannot easily synchronize with internal state,
    // we might need to relax the test or use a slightly larger queue
    // if we can't guarantee startup time.
    
    // Retry with queue size 2 to allow:
    // 1. Task 1 (Running)
    // 2. Task 2 (Queued)
    // 3. Task 3 (Queued, if size 2) 
    // Wait, let's just make sure we interpret "queue_size" correctly.
    // In threadpool.c: if (pool->queue_count >= pool->queue_size) return -1.
    // queue_count includes tasks waiting in the list.
    // When worker takes a task, it decrements queue_count.
    
    // Issue: Task 1 submitted -> queue_count = 1. Worker needs to wake up and decrement.
    // If we submit Task 2 immediately, worker might not have decremented yet.
    // So queue_count is still 1. 1 >= 1 -> Full.
    
    // Fix: Use usleep to give worker time to pick up Task 1.
    
    pool = tp_create(1, 1);
    
    TEST_ASSERT_EQUAL_INT(0, tp_submit(pool, slow_task, NULL));
    
    // Give worker time to pick up the task and decrement queue_count
    usleep(10000); // 10ms
    
    // Now queue should be empty (0), but thread is busy.
    // Submit task 2 -> Should succeeed (queue_count becomes 1)
    TEST_ASSERT_EQUAL_INT(0, tp_submit(pool, increment_task, NULL));
    
    // Submit task 3 -> Should fail (queue_count is 1, size is 1)
    TEST_ASSERT_EQUAL_INT(-1, tp_submit(pool, increment_task, NULL));
    
    tp_destroy(pool, false);
    pool = NULL;
}

TEST(THREADPOOL, GracefulShutdown) {
    pool = tp_create(4, 10);
    
    for (int i = 0; i < 10; i++) {
        tp_submit(pool, increment_task, NULL);
    }
    
    tp_destroy(pool, true);
    pool = NULL;
    
    TEST_ASSERT_EQUAL_INT(10, atomic_load(&counter));
}

TEST_GROUP_RUNNER(THREADPOOL) {
    RUN_TEST_CASE(THREADPOOL, CreateDestroy);
    RUN_TEST_CASE(THREADPOOL, InvalidCreate);
    RUN_TEST_CASE(THREADPOOL, SubmitAndExecute);
    RUN_TEST_CASE(THREADPOOL, QueueFull);
    RUN_TEST_CASE(THREADPOOL, GracefulShutdown);
}
