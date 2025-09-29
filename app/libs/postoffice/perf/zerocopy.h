/**
 * @file perf/zerocopy.h
 * @ingroup perf
 * @brief Fixed-size zero-copy buffer pool backed by (attempted) contiguous
 *        2 MiB huge page mappings for reduced TLB pressure.
 *
 * The pool allocates a contiguous memory region sized for `buf_count * buf_size`
 * bytes and rounded up to a 2 MiB multiple. It first attempts to obtain 2 MiB
 * huge pages (`MAP_HUGETLB`) and transparently falls back to standard pages if
 * that fails. The region is then subdivided into @c buf_count equally sized,
 * naturally aligned buffers which are pushed into an internal single-producer /
 * single-consumer ring of free pointers (see @ref perf/ringbuf.h).
 *
 * Characteristics / constraints:
 *  - All buffers are fixed-size; partial allocation is not supported.
 *  - Allocation (`perf_zcpool_acquire`) is non-blocking: returns NULL and sets
 *    `errno = EAGAIN` if no free buffers remain.
 *  - Release validates that the pointer lies exactly on a buffer boundary;
 *    invalid pointers are silently ignored (defensive robustness).
 *  - Thread model: underlying free queue is SPSC; multiple producers /
 *    consumers require external synchronization around acquire / release.
 *  - Intended for high-throughput network framing / serialization where
 *    buffers are filled then handed off without copying.
 *
 * Error codes (mapped via utils/errors.h where present):
 *  - EINVAL / ZCP_EINVAL: invalid arguments (size 0, size > 2 MiB, count < 1).
 *  - ZCP_EMMAP: mapping failure (both huge and normal page attempts failed).
 *  - ZCP_EAGAIN: transient exhaustion (no free buffers on acquire).
 *
 * Lifecycle / ownership:
 *  - Create with ::perf_zcpool_create(); destroy with ::perf_zcpool_destroy().
 *  - After destroy the handle pointer is nulled; outstanding acquired buffers
 *    become invalid (must not be used nor released).
 *
 * @see perf/ringbuf.h
 */
#ifndef _PERF_ZEROCOPY_H
#define _PERF_ZEROCOPY_H

#include <stddef.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque zero‑copy pool handle (see file header for semantics).
typedef struct perf_zcpool perf_zcpool_t;

/**
 * @brief Return configured per-buffer size in bytes.
 *
 * @param pool Pool handle.
 * @return Buffer size (bytes). Undefined if @p pool is NULL.
 */
size_t perf_zcpool_bufsize(const perf_zcpool_t *pool) __nonnull((1));

/**
 * @brief Create a zero-copy pool (attempt hugepage-backed) of fixed-size buffers.
 *
 * @param buf_count Number of buffers (>=1).
 * @param buf_size  Size of each buffer (1 .. 2 MiB inclusive).
 * @return New pool handle or NULL on failure (errno/ZCP_* set as described in
 *         file header). On failure no resources leak.
 */
perf_zcpool_t *perf_zcpool_create(size_t buf_count, size_t buf_size);

/**
 * @brief Destroy a pool, unmapping its region and releasing metadata.
 *
 * Safe to call with an already NULL *pool (no-op). After return the pointer
 * is set to NULL. Outstanding buffers must not be reused nor released.
 */
void perf_zcpool_destroy(perf_zcpool_t **pool) __nonnull((1));

/**
 * @brief Non-blocking attempt to obtain one free buffer.
 *
 * @param pool Pool handle.
 * @return Pointer to a zeroed (not guaranteed) buffer on success, or NULL with
 *         errno = EAGAIN / ZCP_EAGAIN when exhausted, errno = EINVAL on misuse.
 */
void *perf_zcpool_acquire(perf_zcpool_t *pool) __nonnull((1));

/**
 * @brief Return a buffer to the free pool.
 *
 * Silent no-op if @p buffer is not a valid buffer start for this pool.
 * Undefined behavior if a buffer is released more than once without an
 * intervening successful acquire (the implementation does not currently
 * detect double-frees).
 *
 * @param pool   Pool handle.
 * @param buffer Previously acquired buffer pointer.
 */
void perf_zcpool_release(perf_zcpool_t *pool, void *buffer) __nonnull((1, 2));

/**
 * @brief Advisory count of currently free buffers.
 *
 * @param pool Pool handle.
 * @return Number of buffers that could be acquired without blocking at the
 *         instant of the call (stale under concurrent activity).
 */
size_t perf_zcpool_freecount(const perf_zcpool_t *pool) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // _PERF_ZEROCOPY_H
