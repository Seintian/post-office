/** \file task_queue.h
 *  \ingroup director
 *  \brief Task queue supporting MPSC operations via SPSC ringbuf + spinlock.
 */
#ifndef PO_DIRECTOR_TASK_QUEUE_H
#define PO_DIRECTOR_TASK_QUEUE_H

#include <pthread.h>
#include <postoffice/perf/ringbuf.h>
#include <postoffice/perf/zerocopy.h>
#include <postoffice/perf/cache.h>

/**
 * \brief A generic task is a function pointer + context.
 */
typedef void (*po_task_fn)(void *ctx);

/**
 * \brief Task queue structure.
 * Wraps a perf_ringbuf_t with a spinlock to allow multiple producers.
 * Uses a perf_zcpool_t to allocate task nodes without malloc.
 * 
 * Cache line padding prevents false sharing between fields accessed by
 * different threads. We use PO_CACHE_LINE_MAX (128 bytes) to support
 * architectures with larger cache lines (e.g., some PowerPC systems).
 * This wastes memory on x86-64 (64-byte lines) but ensures correctness
 * everywhere.
 */

typedef struct {
    po_perf_ringbuf_t *ring;
    char _pad1[PO_CACHE_LINE_MAX - sizeof(void*)];  // Isolate ring pointer

    perf_zcpool_t *pool;
    char _pad2[PO_CACHE_LINE_MAX - sizeof(void*)];  // Isolate pool pointer

    pthread_spinlock_t lock;
    char _pad3[PO_CACHE_LINE_MAX - sizeof(pthread_spinlock_t)];  // Isolate lock
} po_task_queue_t;

/**
 * \brief Initialize the task queue.
 * \param queue Pointer to the queue structure.
 * \param capacity Queue capacity (must be power of two).
 * \return 0 on success, -1 on failure.
 */
int po_task_queue_init(po_task_queue_t *queue, size_t capacity);

/**
 * \brief Enqueue a task (Thread-safe, Supports Multiple Producers).
 * Uses a spinlock to serialize writes to the underlying SPSC ring.
 * \param queue Pointer to the queue.
 * \param fn Function to execute.
 * \param ctx Context pointer for the function.
 * \return 0 on success, -1 if full.
 */
int po_task_enqueue(po_task_queue_t *queue, po_task_fn fn, void *ctx);

/**
 * \brief Drain tasks from the queue (Single Consumer only).
 * \param queue Pointer to the queue.
 * \param max_tasks Maximum number of tasks to process.
 * \return Number of tasks processed.
 */
size_t po_task_drain(po_task_queue_t *queue, size_t max_tasks);

/**
 * \brief Clean up queue resources.
 */
void po_task_queue_destroy(po_task_queue_t *queue);

#endif /* PO_DIRECTOR_TASK_QUEUE_H */
