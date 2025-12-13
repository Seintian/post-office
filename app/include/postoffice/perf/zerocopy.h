#ifndef PO_PERF_ZEROCOPY_H
#define PO_PERF_ZEROCOPY_H

#include <stddef.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct perf_zcpool perf_zcpool_t;

typedef enum {
    PERF_ZCPOOL_NOFLAGS = 0,
    PERF_ZCPOOL_METRICS = 1 << 0
} perf_zcpool_flags_t;

/**
 * Create a zerocopy pool.
 *
 * @param buf_count Number of buffers.
 * @param buf_size Size of each buffer.
 * @param flags Creation flags.
 * @return Pointer to pool or NULL.
 */
perf_zcpool_t *perf_zcpool_create(size_t buf_count, size_t buf_size, perf_zcpool_flags_t flags);

/**
 * Destroy a zerocopy pool.
 *
 * @param p Double pointer to the pool.
 */
void perf_zcpool_destroy(perf_zcpool_t **p);

/**
 * Acquire a buffer from the pool.
 *
 * @param p The pool.
 * @return Pointer to the buffer, or NULL if empty/error.
 */
void *perf_zcpool_acquire(perf_zcpool_t *p);

/**
 * Release a buffer back to the pool.
 *
 * @param p The pool.
 * @param buffer The buffer to release.
 */
void perf_zcpool_release(perf_zcpool_t *p, void *buffer);

/**
 * Get the size of buffers in the pool.
 */
size_t perf_zcpool_bufsize(const perf_zcpool_t *p);

/**
 * Get the number of free buffers in the pool.
 */
size_t perf_zcpool_freecount(const perf_zcpool_t *p);

#ifdef __cplusplus
}
#endif

#endif // PO_PERF_ZEROCOPY_H
