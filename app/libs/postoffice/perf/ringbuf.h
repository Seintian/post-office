#ifndef _PERF_RINGBUF_H
#define _PERF_RINGBUF_H

#include <stddef.h>
#include <stdatomic.h>


#ifdef __cplusplus
extern "C" {
#endif

/// Opaque ring‐buffer type
typedef struct _perf_ringbuf_t perf_ringbuf_t;

/**
 * @brief Tell the ring‐buffer module what your hardware cache‐line size is.
 *
 * Must be called once before creating any ring buffers, if you want to
 * ensure head/tail live in different cache lines. Defaults to 64.
 */
void perf_ringbuf_set_cacheline(size_t cacheline_bytes);

/**
 * @brief Create a ring buffer with the given capacity (must be power‐of‐two).
 *
 * @param capacity Number of slots (power‐of‐two)
 * @return pointer or NULL on failure
 */
perf_ringbuf_t *perf_ringbuf_create(size_t capacity);

/**
 * @brief Destroy a ring buffer (frees all internal memory).
 */
void perf_ringbuf_destroy(perf_ringbuf_t **rb);

/**
 * @brief Enqueue one pointer into the ring.
 * @return 0 on success, -1 if the ring is full.
 */
int perf_ringbuf_enqueue(perf_ringbuf_t *rb, void *item);

/**
 * @brief Dequeue one pointer from the ring.
 * @return 0 on success (*out set), -1 if the ring is empty.
 */
int perf_ringbuf_dequeue(perf_ringbuf_t *rb, void **out);

/**
 * @brief Return the approximate number of items currently in the ring.
 */
size_t perf_ringbuf_count(const perf_ringbuf_t *rb);

#ifdef __cplusplus
}
#endif

#endif // _PERF_RINGBUF_H
