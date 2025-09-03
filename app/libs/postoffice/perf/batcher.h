#ifndef _PERF_BATCHER_H
#define _PERF_BATCHER_H

#include <sys/types.h>
#include <stdbool.h>
#include "perf/ringbuf.h"


#ifdef __cplusplus
extern "C" {
#endif

/// Opaque batcher type
typedef struct perf_batcher perf_batcher_t;

/**
 * @brief Create a blocking batcher over a ring buffer.
 *
 * The batcher takes ownership of the ring buffer for enqueue,
 * combining enqueue + eventfd wakeup. Consumption blocks in next().
 *
 * @param rb         Underlying ring buffer (must be non-NULL)
 * @param batch_size Maximum items to pull per batch (must be >=1)
 * @return           New batcher or NULL on failure (errno set)
 * 
 * @note The batcher must be freed by the caller using `free(batcher)`.
 *       It does not free itself in `perf_batcher_destroy()` to allow
 *       the caller to detect invalid state (e.g. if the batcher was
 *       already destroyed).
 */
perf_batcher_t *perf_batcher_create(perf_ringbuf_t *rb, size_t batch_size) __nonnull((1));

/**
 * @brief Destroy a batcher, free resources.
 * 
 * @note This doesn't free the underlying ring buffer.
 */
void perf_batcher_destroy(perf_batcher_t **b) __nonnull((1));

/**
 * @brief Enqueue one item into the batcher.
 *
 * Internally writes to ring buffer and signals via eventfd.
 *
 * @return 0 on success, -1 on ring-full or other error (errno set)
 */
int perf_batcher_enqueue(perf_batcher_t *b, void *item) __nonnull((1, 2));

/**
 * @brief Flush the batcher, ensuring all items are processed.
 *
 * This will block until all items in the ring buffer are consumed.
 * Useful for ensuring all pending items are processed before shutdown.
 *
 * @param b   Batcher handle
 * @param fd  File descriptor to signal completion (e.g. eventfd)
 * @return    0 on success, -1 on error (errno set)
 */
int perf_batcher_flush(perf_batcher_t *b, int fd) __nonnull((1));

/**
 * @brief Dequeue up to batch_size items, blocking until at least one is available.
 *
 * @param b   Batcher handle
 * @param out Array of pointers, length >= batch_size
 * @return    Number of items dequeued (>=1), or -1 on error (errno set)
 */
ssize_t perf_batcher_next(perf_batcher_t *b, void **out) __nonnull((1, 2));

/**
 * @brief Check if the batcher is empty (no items queued).
 *
 * @param b Batcher handle
 * @return true if empty, false if there are items queued.
 */
bool perf_batcher_is_empty(const perf_batcher_t *b) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // _PERF_BATCHER_H
