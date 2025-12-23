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
 * @brief Create a new batcher.
 *
 * @param[in] rb The underlying ring buffer to wrap (must not be NULL).
 * @param[in] batch_size Max items to batch.
 * @param[in] flags Creation flags.
 * @return Pointer to batcher or NULL on failure.
 *
 * @note Thread-safe: Yes (Creation).
 */
po_perf_batcher_t *perf_batcher_create(po_perf_ringbuf_t *rb, size_t batch_size, perf_batcher_flags_t flags);

/**
 * @brief Destroy a batcher.
 *
 * @param[in,out] b Double pointer to the batcher. Sets *b to NULL.
 *
 * @note Thread-safe: No (Must be exclusive).
 */
void perf_batcher_destroy(po_perf_batcher_t **b);

/**
 * @brief Enqueue an item via the batcher (signals consumer).
 *
 * @param[in] b The batcher (must not be NULL).
 * @param[in] item The item to enqueue (opaque pointer).
 * @return 0 on success, -1 on failure.
 *
 * @note Thread-safe: No (Single Producer only, unless underlying ringbuf is MP-safe).
 */
int perf_batcher_enqueue(po_perf_batcher_t *b, void *item);

/**
 * @brief Dequeue a batch of items.
 *
 * @param[in] b The batcher (must not be NULL).
 * @param[out] out Array of pointers to store items.
 * @return Number of items dequeued (can be 0), or -1 on error.
 *
 * @note Thread-safe: No (Single Consumer only).
 */
ssize_t perf_batcher_next(po_perf_batcher_t *b, void **out);

/**
 * @brief Flush pending items to a file descriptor (e.g. socket).
 *
 * @param[in] b The batcher (must not be NULL).
 * @param[in] fd File descriptor.
 * @return 0 on success, -1 on error.
 *
 * @note Thread-safe: No (Single Consumer only).
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
