/** \file task_queue.c
 *  \ingroup director
 *  \brief Implementation of the MPSC task queue using SPSC ringbuf + Fixed-Size Pool.
 */

#define _POSIX_C_SOURCE 200809L
#include "task_queue.h"
#include <postoffice/metrics/metrics.h>
#include <stdlib.h>
#include <errno.h>

/* Internal structure for the ring buffer payload */
typedef struct {
    po_task_fn fn;
    void *ctx;
} task_node_t;

int po_task_queue_init(po_task_queue_t *queue, size_t capacity) {
    if (!queue || capacity == 0) return -1;
    
    // 1. Create Ring Buffer (SPSC)
    queue->ring = perf_ringbuf_create(capacity, PERF_RINGBUF_METRICS);
    if (!queue->ring) {
        return -1;
    }

    // 2. Create Node Pool (SPSC)
    size_t pool_size = capacity * 2;
    if (pool_size < 16) pool_size = 16;
    
    queue->pool = perf_zcpool_create(pool_size, sizeof(task_node_t), PERF_ZCPOOL_METRICS);
    if (!queue->pool) {
        perf_ringbuf_destroy(&queue->ring);
        return -1;
    }

    // 3. Initialize Lock
    if (pthread_spin_init(&queue->lock, PTHREAD_PROCESS_PRIVATE) != 0) {
        perf_zcpool_destroy(&queue->pool);
        perf_ringbuf_destroy(&queue->ring);
        return -1;
    }

    // Register metrics
    PO_METRIC_COUNTER_CREATE("director.queue.full");
    PO_METRIC_COUNTER_CREATE("director.queue.drop");
    PO_METRIC_COUNTER_CREATE("director.queue.pool_exhausted");

    return 0;
}

int po_task_enqueue(po_task_queue_t *queue, po_task_fn fn, void *ctx) {
    if (!queue || !fn) return -1;

    // Critical Section: MPSC requires a lock.
    pthread_spin_lock(&queue->lock);

    task_node_t *node = (task_node_t *)perf_zcpool_acquire(queue->pool);
    if (!node) {
        PO_METRIC_COUNTER_INC("director.queue.pool_exhausted");
        PO_METRIC_COUNTER_INC("director.queue.drop");
        pthread_spin_unlock(&queue->lock);
        return -1;
    }

    node->fn = fn;
    node->ctx = ctx;

    int rc = perf_ringbuf_enqueue(queue->ring, node);
    if (rc != 0) {
        // Queue full. Return node to pool immediately while under lock.
        perf_zcpool_release(queue->pool, node);
        PO_METRIC_COUNTER_INC("director.queue.full");
        PO_METRIC_COUNTER_INC("director.queue.drop");
        pthread_spin_unlock(&queue->lock);
        return -1;
    }

    pthread_spin_unlock(&queue->lock);
    return 0;
}

size_t po_task_drain(po_task_queue_t *queue, size_t max_tasks) {
    if (!queue) return 0;

    size_t processed = 0;
    void *item = NULL;
    
    // Local batch of nodes to release to pool
    #define BATCH_LIMIT 256
    void *nodes_to_free[BATCH_LIMIT];
    size_t pending_free = 0;

    // SPSC/MPSC Consumer Logic: Lock-Free
    while (processed < max_tasks) {
        if (perf_ringbuf_dequeue(queue->ring, &item) == 0) {
            task_node_t *node = (task_node_t*)item;
            
            // Execute task
            if (node && node->fn) {
                node->fn(node->ctx);
            }
            
            // Stash for release
            if (node) {
                nodes_to_free[pending_free++] = node;
            }
            processed++;

            // Flush free batch if full
            if (pending_free >= BATCH_LIMIT) {
                pthread_spin_lock(&queue->lock);
                for (size_t i = 0; i < pending_free; i++) {
                    perf_zcpool_release(queue->pool, nodes_to_free[i]);
                }
                pthread_spin_unlock(&queue->lock);
                pending_free = 0;
            }

        } else {
            // Queue empty
            break;
        }
    }
    
    // Flush remaining frees
    if (pending_free > 0) {
        pthread_spin_lock(&queue->lock);
        for (size_t i = 0; i < pending_free; i++) {
            perf_zcpool_release(queue->pool, nodes_to_free[i]);
        }
        pthread_spin_unlock(&queue->lock);
    }
    
    return processed;
}

void po_task_queue_destroy(po_task_queue_t *queue) {
    if (!queue) return;
    
    // Drain remaining tasks
    po_task_drain(queue, 10000);

    // Now destroy components
    if (queue->ring) {
        perf_ringbuf_destroy(&queue->ring);
    }
    if (queue->pool) {
        perf_zcpool_destroy(&queue->pool);
    }
    pthread_spin_destroy(&queue->lock);
}
