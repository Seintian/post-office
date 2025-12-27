#include "concurrency/threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

typedef struct tp_task_s {
    tp_task_func_t func;
    void *arg;
    struct tp_task_s *next;
} tp_task_t;

struct threadpool_s {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    tp_task_t *queue_head;
    tp_task_t *queue_tail;
    size_t num_threads;
    size_t threads_started;
    size_t queue_size;
    size_t queue_count;
    bool shutdown;
    bool graceful;
    atomic_uint *external_active_counter;
};

/**
 * @brief Worker thread entry point.
 * @param[in] arg Threadpool instance pointer.
 * @return NULL.
 */
static void* tp_worker(void *arg) {
    threadpool_t *pool = (threadpool_t*)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        while ((pool->queue_count == 0) && (!pool->shutdown)) {
            pthread_cond_wait(&pool->notify, &pool->lock);
        }

        if (pool->shutdown && (!pool->graceful || pool->queue_count == 0)) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        // Grab task
        tp_task_t *task = pool->queue_head;
        if (task) {
            pool->queue_head = task->next;
            if (pool->queue_head == NULL) {
                pool->queue_tail = NULL;
            }
            pool->queue_count--;
        }

        pthread_mutex_unlock(&pool->lock);

        if (task) {
            if (pool->external_active_counter) {
                atomic_fetch_add(pool->external_active_counter, 1);
            }
            task->func(task->arg);
            if (pool->external_active_counter) {
                atomic_fetch_sub(pool->external_active_counter, 1);
            }
            free(task);
        }
    }

    return NULL;
}

threadpool_t* tp_create(size_t num_threads, size_t queue_size) {
    if (num_threads == 0) return NULL;

    threadpool_t *pool = (threadpool_t*)calloc(1, sizeof(threadpool_t));
    int mutex_ok = (pthread_mutex_init(&pool->lock, NULL) == 0);
    if (!mutex_ok || pthread_cond_init(&pool->notify, NULL) != 0) {
        if (mutex_ok) {
            pthread_mutex_destroy(&pool->lock);
        }
        free(pool);
        return NULL;
    }
    pool->num_threads = num_threads;
    pool->queue_size = (queue_size == 0) ? SIZE_MAX : queue_size;

    pool->threads = (pthread_t*)calloc(num_threads, sizeof(pthread_t));
    if (!pool->threads) {
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->notify);
        free(pool);
        return NULL;
    }

    for (size_t i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, tp_worker, pool) != 0) {
            tp_destroy(pool, 0);
            return NULL;
        }
        pool->threads_started++;
    }

    return pool;
}

int tp_submit(threadpool_t *pool, tp_task_func_t func, void *arg) {
    if (!pool || !func) return -1;

    pthread_mutex_lock(&pool->lock);

    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    if (pool->queue_count >= pool->queue_size) {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    tp_task_t *task = (tp_task_t*)malloc(sizeof(tp_task_t));
    if (!task) {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }
    task->func = func;
    task->arg = arg;
    task->next = NULL;

    if (pool->queue_tail) {
        pool->queue_tail->next = task;
        pool->queue_tail = task;
    } else {
        pool->queue_head = task;
        pool->queue_tail = task;
    }
    pool->queue_count++;

    pthread_cond_signal(&pool->notify);
    pthread_mutex_unlock(&pool->lock);
    return 0;
}

void tp_destroy(threadpool_t *pool, bool graceful) {
    if (!pool) return;

    pthread_mutex_lock(&pool->lock);
    pool->shutdown = true;
    pool->graceful = graceful;
    pthread_cond_broadcast(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    for (size_t i = 0; i < pool->threads_started; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // Cleanup remaining tasks
    pthread_mutex_lock(&pool->lock);
    tp_task_t *curr = pool->queue_head;
    while (curr) {
        tp_task_t *next = curr->next;
        free(curr);
        curr = next;
    }
    pthread_mutex_unlock(&pool->lock);

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->notify);
    free(pool->threads);
    free(pool);
}

void tp_set_active_counter(threadpool_t *pool, atomic_uint *counter) {
    if (pool) {
        pool->external_active_counter = counter;
    }
}
