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
 * @brief Create a new ring buffer.
 *
 * @param[in] capacity Capacity of the ring buffer (must be a power of two). All slots are usable.
 * @param[in] flags Creation flags (e.g. to enable metrics).
 * @return Pointer to the new ring buffer, or NULL on failure.
 *
 * @note Thread-safe: Yes (Creation).
 */
po_perf_ringbuf_t *perf_ringbuf_create(size_t capacity, perf_ringbuf_flags_t flags);

/**
 * @brief Destroy a ring buffer.
 *
 * @param[in,out] rb Double pointer to the ring buffer (sets *rb to NULL).
 *
 * @note Thread-safe: No (Must be exclusive).
 */
void perf_ringbuf_destroy(po_perf_ringbuf_t **rb);

/**
 * @brief Enqueue an item into the ring buffer.
 *
 * @param[in] rb The ring buffer (must not be NULL).
 * @param[in] item The item to enqueue (opaque pointer).
 * @return 0 on success, -1 on failure (errno set to EAGAIN if full).
 *
 * @note Thread-safe: No (Single Producer Only).
 */
int perf_ringbuf_enqueue(po_perf_ringbuf_t *restrict rb, void *item);

/**
 * @brief Dequeue an item from the ring buffer.
 *
 * @param[in] rb The ring buffer (must not be NULL).
 * @param[out] out Pointer to store the dequeued item (can be NULL).
 * @return 0 on success, -1 on failure (empty).
 *
 * @note Thread-safe: No (Single Consumer Only).
 */
int perf_ringbuf_dequeue(po_perf_ringbuf_t *restrict rb, void **restrict out);

/**
 * @brief Get the number of items currently in the ring buffer.
 *
 * @param[in] rb The ring buffer (must not be NULL).
 * @return Number of items.
 *
 * @note Thread-safe: Yes (Approximate if concurrent access).
 */
size_t perf_ringbuf_count(const po_perf_ringbuf_t *restrict rb);

/**
 * @brief Set the cacheline size for alignment.
 *
 * @param[in] cacheline_bytes Size in bytes (default 64).
 *
 * @note Thread-safe: No (Should be called before creation).
 */
void perf_ringbuf_set_cacheline(size_t cacheline_bytes);

/**
 * @brief Peek at the item at the head of the ring buffer without removing it.
 *
 * @param[in] rb The ring buffer (must not be NULL).
 * @param[out] out Pointer to store the item.
 * @return 0 on success, -1 if empty.
 *
 * @note Thread-safe: No (Single Consumer Only).
 */
int perf_ringbuf_peek(const po_perf_ringbuf_t *restrict rb, void **restrict out);

/**
 * @brief Peek at an item at a specific offset from the head.
 *
 * @param[in] rb The ring buffer (must not be NULL).
 * @param[in] idx Offset from head (0 = head).
 * @param[out] out Pointer to store the item.
 * @return 0 on success, -1 if idx >= count.
 *
 * @note Thread-safe: No (Single Consumer Only).
 */
int perf_ringbuf_peek_at(const po_perf_ringbuf_t *restrict rb, size_t idx, void **restrict out);

/**
 * @brief Advance the head of the ring buffer (discard items).
 *
 * @param[in] rb The ring buffer (must not be NULL).
 * @param[in] n Number of items to discard.
 * @return 0 on success, -1 if n > count.
 *
 * @note Thread-safe: No (Single Consumer Only).
 */
int perf_ringbuf_advance(po_perf_ringbuf_t *restrict rb, size_t n);

#ifdef __cplusplus
}
#endif

#endif // PO_PERF_RINGBUF_H
