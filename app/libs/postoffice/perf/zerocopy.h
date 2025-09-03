/** \ingroup perf */
#ifndef _PERF_ZEROCOPY_H
#define _PERF_ZEROCOPY_H

#include <stddef.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque zero‑copy pool handle
typedef struct perf_zcpool perf_zcpool_t;

/**
 * @brief Get the size of each buffer in the zero-copy pool.
 *
 * @param pool The pool to query.
 * @return Size of each buffer in bytes, or 0 if pool is NULL.
 */
size_t perf_zcpool_bufsize(const perf_zcpool_t *pool) __nonnull((1));

/**
 * @brief Create a zero‑copy buffer pool backed by 2 MiB huge pages.
 *
 * Allocates a contiguous region of `buf_count * buf_size` bytes using
 * 2 MiB huge pages (if available), and builds a free‑list over the `buf_count`
 * fixed‑size buffers. The pool is non‑blocking: acquire() returns NULL if
 * no buffers are free.
 *
 * @param buf_count Number of buffers in the pool (must be ≥1)
 * @param buf_size  Size of each buffer in bytes (must be ≤ 2 MiB)
 * @return          New pool handle or NULL on failure (errno set)
 */
perf_zcpool_t *perf_zcpool_create(size_t buf_count, size_t buf_size);

/**
 * @brief Destroy a zero‑copy pool, free all resources.
 *
 * @param pool The pool to destroy.
 */
void perf_zcpool_destroy(perf_zcpool_t **pool) __nonnull((1));

/**
 * @brief Acquire one free buffer from the pool.
 *
 * @param pool The pool to acquire from.
 * @return     Pointer to a buffer, or NULL if none are free (errno=EAGAIN).
 */
void *perf_zcpool_acquire(perf_zcpool_t *pool) __nonnull((1));

/**
 * @brief Release a buffer back to the pool.
 *
 * The pointer must be one previously returned by acquire() on this pool.
 *
 * @param pool   The pool to release into.
 * @param buffer Buffer pointer to return.
 */
void perf_zcpool_release(perf_zcpool_t *pool, void *buffer) __nonnull((1, 2));

/**
 * @brief Get the number of free buffers currently available.
 *
 * @param pool The pool to query.
 * @return     Number of free buffers, or 0 if pool is NULL.
 */
size_t perf_zcpool_freecount(const perf_zcpool_t *pool) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // _PERF_ZEROCOPY_H
