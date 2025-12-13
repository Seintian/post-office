#ifndef PO_PERF_BATCHER_H
#define PO_PERF_BATCHER_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct perf_batcher po_perf_batcher_t;
typedef struct perf_ringbuf po_perf_ringbuf_t;

typedef enum {
    PERF_BATCHER_NOFLAGS = 0,
    PERF_BATCHER_METRICS = 1 << 0
} perf_batcher_flags_t;

/**
 * Create a new batcher.
 *
 * @param rb The underlying ring buffer to wrap.
 * @param batch_size Max items to batch.
 * @param flags Creation flags.
 * @return Pointer to batcher or NULL.
 */
po_perf_batcher_t *perf_batcher_create(po_perf_ringbuf_t *rb, size_t batch_size, perf_batcher_flags_t flags);

/**
 * Destroy a batcher.
 *
 * @param b Double pointer to the batcher.
 */
void perf_batcher_destroy(po_perf_batcher_t **b);

/**
 * Enqueue an item via the batcher (signals consumer).
 *
 * @param b The batcher.
 * @param item The item to enqueue.
 * @return 0 on success, -1 on failure.
 */
int perf_batcher_enqueue(po_perf_batcher_t *b, void *item);

/**
 * Dequeue a batch of items.
 *
 * @param b The batcher.
 * @param out Array of pointers to store items.
 * @return Number of items dequeued (can be 0), or -1 on error.
 */
ssize_t perf_batcher_next(po_perf_batcher_t *b, void **out);

/**
 * Flush pending items to a file descriptor (e.g. socket).
 *
 * @param b The batcher.
 * @param fd File descriptor.
 * @return 0 on success, -1 on error.
 */
int perf_batcher_flush(po_perf_batcher_t *b, int fd);

/**
 * Check if the batcher is empty.
 *
 * @param b The batcher.
 * @return true if empty, false otherwise.
 */
bool perf_batcher_is_empty(const po_perf_batcher_t *b);

#ifdef __cplusplus
}
#endif

#endif // PO_PERF_BATCHER_H
