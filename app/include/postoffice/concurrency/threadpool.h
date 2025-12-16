#ifndef PO_CONCURRENCY_THREADPOOL_H
#define PO_CONCURRENCY_THREADPOOL_H

#include <stddef.h>
#include <stdbool.h>

typedef struct threadpool_s threadpool_t;

/**
 * @brief Task function signature.
 */
typedef void (*tp_task_func_t)(void *arg);

/**
 * @brief Initialize a thread pool.
 * 
 * @param num_threads Number of worker threads to spawn.
 * @param queue_size Maximum number of pending tasks (0 = unlimited/large default).
 * @return threadpool_t* Pointer to pool, or NULL on failure.
 */
threadpool_t* tp_create(size_t num_threads, size_t queue_size);

/**
 * @brief Submit a task to the pool.
 * 
 * @param pool Pool instance.
 * @param func Function to execute.
 * @param arg Argument to pass to function.
 * @return 0 on success, -1 on failure (queue full or shutdown).
 */
int tp_submit(threadpool_t *pool, tp_task_func_t func, void *arg);

/**
 * @brief Shutdown the pool.
 * 
 * @param pool Pool instance.
 * @param graceful If true, wait for pending tasks to finish.
 */
void tp_destroy(threadpool_t *pool, bool graceful);

#endif // PO_CONCURRENCY_THREADPOOL_H
