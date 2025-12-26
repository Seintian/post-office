/** \file ringbuf.h
 *  \ingroup perf
 *  \brief Lock-free single-producer / single-consumer ring buffer used for
 *         staging events, samples or pointers between hot producers and
 *         background consumers in the perf subsystem.
 *
 *  Design
 *  ------
 *  - Power-of-two capacity enabling mask-based wrap (idx & (cap-1)).
 *  - Separate head (consumer) / tail (producer) indices with atomic loads &
 *    stores; release on publish, acquire on consume.
 *  - Padding (optional) to isolate indices onto different cache lines when
 *    `perf_ringbuf_set_cacheline()` is invoked early.
 *  - Items are opaque `void*` to keep the buffer generic; ownership/ lifetime
 *    is defined by higher layers (e.g., perf batching, zerocopy pools).
 *
 *  Concurrency Model
 *  -----------------
 *  The implementation targets SPSC. For MPSC use an external combiner or
 *  a spinlock around enqueue. For SPMC add external synchronization around
 *  dequeue or partition work. Multi-producer misuse will manifest as lost
 *  writes or torn indices.
 *
 *  Memory Ordering
 *  ---------------
 *  Enqueue performs: load head (acquire) -> compute space -> store item ->
 *  release store of tail. Dequeue performs: load tail (acquire) -> check
 *  emptiness -> read item -> release store of head. This ensures a consumer
 *  always sees a fully initialized pointer after successful dequeue.
 *
 *  Errors
 *  ------
 *  - `perf_ringbuf_create`: EINVAL (non power-of-two / too small), ENOMEM.
 *  - Enqueue: returns -1 if full (callers typically count drops).
 *  - Dequeue: returns -1 if empty.
 *
 *  Performance Notes
 *  -----------------
 *  - Count / peek functions are approximate in concurrent contexts.
 *  - Avoid interleaving frequent count calls on hot paths; maintain an
 *    external statistic if precise accounting is required.
 *  - Capacity should reflect expected burst size to minimize full hits; keep
 *    it under LLC footprint to avoid eviction of working set.
 *
 *  Related
 *  -------
 *  @see batcher.h for higher-level batching fed by ring buffers.
 *  @see zerocopy.h for pooled allocation patterns often enqueued.
 */
#ifndef _PERF_RINGBUF_H
#define _PERF_RINGBUF_H

#include <stdatomic.h>
#include <stddef.h>

#include "sys/cdefs.h" // for __nonnull

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque ring‐buffer type (single-producer / single-consumer semantics unless
/// otherwise documented; multi-producer usage requires external synchronization).
typedef struct perf_ringbuf po_perf_ringbuf_t;

/**
 * @brief Provide hardware cache line size for padding/alignment optimizations.
 *
 * Call once prior to `perf_ringbuf_create()` to reduce false sharing between
 * head/tail indices. If not called, a conservative default (64 bytes) is used.
 */
void perf_ringbuf_set_cacheline(size_t cacheline_bytes);

/**
 * @brief Create a ring buffer with the given capacity (power-of-two required).
 *
 * Memory ordering: The implementation uses atomic operations to publish
 * enqueues before making them visible to consumers. Consumers observe
 * fully initialized item pointers after a successful dequeue.
 *
 * @param capacity Number of slots (must be >= 2 and power-of-two).
 * @return New ring buffer handle or NULL on failure (errno set: EINVAL, ENOMEM).
 */
po_perf_ringbuf_t *perf_ringbuf_create(size_t capacity);

/**
 * @brief Destroy a ring buffer (frees all internal memory) and NULLs the handle.
 */
void perf_ringbuf_destroy(po_perf_ringbuf_t **restrict rb) __nonnull((1));

/**
 * @brief Enqueue one pointer into the ring (non-blocking).
 * @param rb   Ring buffer handle.
 * @param item Pointer value to store (may be NULL if consumer tolerates it).
 * @return 0 on success, -1 if full.
 */
int perf_ringbuf_enqueue(po_perf_ringbuf_t *restrict rb, void *restrict item) __nonnull((1));

/**
 * @brief Dequeue one pointer from the ring (non-blocking).
 * @param rb  Ring buffer handle.
 * @param out Destination for dequeued pointer (must not be NULL).
 * @return 0 on success (*out set), -1 if empty.
 */
int perf_ringbuf_dequeue(po_perf_ringbuf_t *restrict rb, void **restrict out) __nonnull((1));

/**
 * @brief Return the approximate number of items currently in the ring.
 *
 * This value may be momentarily stale in concurrent scenarios.
 */
size_t perf_ringbuf_count(const po_perf_ringbuf_t *restrict rb) __nonnull((1));

/**
 * @brief Peek at the item at the head of the ring without dequeuing.
 * @param rb  Ring buffer handle.
 * @param out Destination for pointer.
 * @return 0 on success (*out set), -1 if empty.
 */
int perf_ringbuf_peek(const po_perf_ringbuf_t *restrict rb, void **restrict out) __nonnull((1));

/**
 * @brief Peek at the item at offset idx from the head (0=head, 1=next, ...).
 * @param rb  Ring buffer handle.
 * @param idx Offset from head.
 * @param out Destination for pointer.
 * @return 0 on success (*out set), -1 if idx >= current count.
 */
int perf_ringbuf_peek_at(const po_perf_ringbuf_t *restrict rb, size_t idx, void **restrict out) __nonnull((1));

/**
 * @brief Advance (drop) exactly n items from the head.
 * @param rb Ring buffer handle.
 * @param n  Number of items to drop.
 * @return 0 on success, -1 if n > current count.
 */
int perf_ringbuf_advance(po_perf_ringbuf_t *restrict rb, size_t n) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // _PERF_RINGBUF_H
