#ifndef PO_PERF_RINGBUF_H
#define PO_PERF_RINGBUF_H

#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct perf_ringbuf po_perf_ringbuf_t;

typedef enum {
    PERF_RINGBUF_NOFLAGS = 0,
    PERF_RINGBUF_METRICS = 1 << 0
} perf_ringbuf_flags_t;

/**
 * Create a new ring buffer.
 *
 * @param capacity Capacity of the ring buffer (must be a power of two).
 * @param flags Creation flags (e.g. to enable metrics).
 * @return Pointer to the new ring buffer, or NULL on failure.
 */
po_perf_ringbuf_t *perf_ringbuf_create(size_t capacity, perf_ringbuf_flags_t flags);

/**
 * Destroy a ring buffer.
 *
 * @param rb Double pointer to the ring buffer (sets *rb to NULL).
 */
void perf_ringbuf_destroy(po_perf_ringbuf_t **rb);

/**
 * Enqueue an item into the ring buffer.
 *
 * @param rb The ring buffer.
 * @param item The item to enqueue (opaque pointer).
 * @return 0 on success, -1 on failure (errno set to EAGAIN if full).
 */
int perf_ringbuf_enqueue(po_perf_ringbuf_t *rb, void *item);

/**
 * Dequeue an item from the ring buffer.
 *
 * @param rb The ring buffer.
 * @param out Pointer to store the dequeued item (can be NULL).
 * @return 0 on success, -1 on failure (empty).
 */
int perf_ringbuf_dequeue(po_perf_ringbuf_t *rb, void **out);

/**
 * Get the number of items currently in the ring buffer.
 *
 * @param rb The ring buffer.
 * @return Number of items.
 */
size_t perf_ringbuf_count(const po_perf_ringbuf_t *rb);

/**
 * Set the cacheline size for alignment.
 *
 * @param cacheline_bytes Size in bytes (default 64).
 */
void perf_ringbuf_set_cacheline(size_t cacheline_bytes);

/**
 * Peek at the item at the head of the ring buffer without removing it.
 *
 * @param rb The ring buffer.
 * @param out Pointer to store the item.
 * @return 0 on success, -1 if empty.
 */
int perf_ringbuf_peek(const po_perf_ringbuf_t *rb, void **out);

/**
 * Peek at an item at a specific offset from the head.
 *
 * @param rb The ring buffer.
 * @param idx Offset from head (0 = head).
 * @param out Pointer to store the item.
 * @return 0 on success, -1 if idx >= count.
 */
int perf_ringbuf_peek_at(const po_perf_ringbuf_t *rb, size_t idx, void **out);

/**
 * Advance the head of the ring buffer (discard items).
 *
 * @param rb The ring buffer.
 * @param n Number of items to discard.
 * @return 0 on success, -1 if n > count.
 */
int perf_ringbuf_advance(po_perf_ringbuf_t *rb, size_t n);

#ifdef __cplusplus
}
#endif

#endif // PO_PERF_RINGBUF_H
