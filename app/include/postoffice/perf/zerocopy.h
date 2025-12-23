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
 * @brief Create a zerocopy pool.
 *
 * @param[in] buf_count Number of buffers.
 * @param[in] buf_size Size of each buffer.
 * @param[in] flags Creation flags.
 * @return Pointer to pool or NULL.
 *
 * @note Thread-safe: Yes (Creation).
 */
perf_zcpool_t *perf_zcpool_create(size_t buf_count, size_t buf_size, perf_zcpool_flags_t flags);

/**
 * @brief Destroy a zerocopy pool.
 *
 * @param[in,out] p Double pointer to the pool. Sets *p to NULL.
 *
 * @note Thread-safe: No (Must be exclusive).
 */
void perf_zcpool_destroy(perf_zcpool_t **p);

/**
 * @brief Acquire a buffer from the pool.
 *
 * @param[in] p The pool (must not be NULL).
 * @return Pointer to the buffer, or NULL if empty/error.
 *
 * @note Thread-safe: No (Single Consumer Only).
 */
void *perf_zcpool_acquire(perf_zcpool_t *p);

/**
 * @brief Release a buffer back to the pool.
 *
 * @param[in] p The pool (must not be NULL).
 * @param[in] buffer The buffer to release.
 *
 * @note Thread-safe: No (Single Producer Only).
 */
void perf_zcpool_release(perf_zcpool_t *p, void *buffer);

/**
 * @brief Get the size of buffers in the pool.
 *
 * @param[in] p The pool (must not be NULL).
 * @return Size of buffers.
 *
 * @note Thread-safe: Yes.
 */
size_t perf_zcpool_bufsize(const perf_zcpool_t *p);

/**
 * @brief Get the number of free buffers in the pool.
 *
 * @param[in] p The pool (must not be NULL).
 * @return Number of free buffers.
 *
 * @note Thread-safe: Yes (Approximate).
 */
size_t perf_zcpool_freecount(const perf_zcpool_t *p);

#ifdef __cplusplus
}
#endif

#endif // PO_PERF_ZEROCOPY_H
