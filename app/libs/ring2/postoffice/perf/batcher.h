/**
 * @file perf/batcher.h
 * @ingroup perf
 * @brief Producer notification + blocking consumer batching helper built on
 *        top of a single-producer / single-consumer ring buffer and an
 *        internal eventfd.
 *
 * The batcher pairs a lightweight pointer ring (see @ref perf/ringbuf.h) with
 * an `eventfd(2)` used purely as a wake / counting semaphore from producer to
 * consumer. Each successful enqueue increments the eventfd counter by 1; the
 * consumer thread blocks inside ::perf_batcher_next() performing a blocking
 * `read(2)` on that eventfd until at least one item is available, after which
 * it drains up to the configured @c batch_size items from the underlying ring
 * in a single call.
 *
 * Threading model:
 *  - Intended for exactly one producer thread calling ::perf_batcher_enqueue()
 *    and one consumer thread calling ::perf_batcher_next().
 *  - The underlying ring buffer is single-producer / single-consumer (SPSC);
 *    using multiple producers or multiple consumers requires external
 *    synchronization (e.g. a mutex) around enqueue / next calls.
 *
 * Ownership / lifetime:
 *  - The caller allocates / owns the ring buffer and passes a pointer to
 *    ::perf_batcher_create(); the batcher never frees it.
 *  - Destroy with ::perf_batcher_destroy() before freeing the ring buffer.
 *
 * Error handling summary:
 *  - ::perf_batcher_create(): sets @c errno = EINVAL (bad args) or propagates
 *    errors from `eventfd` (e.g., EMFILE, ENFILE, ENOMEM).
 *  - ::perf_batcher_enqueue(): on a full ring sets @c errno = EAGAIN; on
 *    signaling failure sets @c errno = EIO; on misuse (destroyed / NULL members)
 *    sets @c errno = EINVAL.
 *  - ::perf_batcher_next(): returns -1 with @c errno if the eventfd read fails
 *    (EBADF if already closed, EINTR if interrupted, etc.). Partial draining
 *    is normal – it returns the number of items actually dequeued (>=1).
 *  - ::perf_batcher_flush(): returns -1 on write / argument errors; see the
 *    detailed notes in its documentation (it is not a full drain guarantee).
 *
 * Performance notes:
 *  - Enqueue is O(1) with a single atomic / index manipulation + an eventfd
 *    write (8 bytes) per item.
 *  - Consumer amortizes the cost of the blocking wake across up to
 *    @c batch_size dequeued items.
 *
 * @see perf/ringbuf.h
 */
#ifndef _PERF_BATCHER_H
#define _PERF_BATCHER_H

#include <stdbool.h>
#include <sys/types.h>

#include "perf/ringbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque batcher type (see file-level docs for semantics)
typedef struct perf_batcher po_perf_batcher_t;

/**
 * @brief Create a batching façade over a caller-managed ring buffer.
 *
 * The ring buffer must satisfy the single-producer / single-consumer usage
 * pattern unless external synchronization is supplied. The batcher does NOT
 * take ownership of @p rb memory – the caller remains responsible for
 * destroying the ring *after* ::perf_batcher_destroy().
 *
 * @param rb         Underlying ring buffer (must be non-NULL).
 * @param batch_size Maximum items to drain per ::perf_batcher_next() call
 *                   (must be >= 1). A larger value increases per-wake
 *                   amortization at the cost of a potentially larger output
 *                   array requirement.
 * @return New batcher instance or NULL on failure (errno set: EINVAL for
 *         invalid args, or errors from eventfd creation such as EMFILE,
 *         ENFILE, ENOMEM).
 */
po_perf_batcher_t *perf_batcher_create(po_perf_ringbuf_t *rb, size_t batch_size) __nonnull((1));

/**
 * @brief Destroy a batcher and close its internal eventfd.
 *
 * Safe to call with an already NULL *b (no-op). The underlying ring buffer
 * passed at creation time is intentionally left untouched so the caller may
 * inspect / drain / reuse it if necessary.
 */
void perf_batcher_destroy(po_perf_batcher_t **restrict b) __nonnull((1));

/**
 * @brief Enqueue one item and signal the consumer.
 *
 * Non-blocking: if the ring is full the call fails immediately with
 * @c errno = EAGAIN. On success, an 8-byte write to the internal eventfd
 * increments the wake counter (EFD_SEMAPHORE mode) so that a blocked
 * consumer thread unblocks (or the count accumulates for later wake ups).
 *
 * @param b    Batcher handle.
 * @param item Pointer payload to enqueue (may be NULL if consumer tolerates).
 * @return 0 on success; -1 on error with @c errno = EAGAIN (full), EINVAL
 *         (invalid batcher state), or EIO (eventfd signaling failure).
 */
int perf_batcher_enqueue(po_perf_batcher_t *restrict b, void *restrict item) __nonnull((1, 2));

/**
 * @brief Attempt a single scatter/gather write of queued frames to @p fd.
 *
 * Semantics (IMPORTANT – this is not a full "drain everything" guarantee):
 *  - Inspects the current ring depth (up to `IOV_MAX`).
 *  - Builds an iovec array treating each queued pointer as the start of a
 *    frame whose first 4 bytes are a uint32_t length prefix.
 *  - Issues one `writev(2)` call.
 *  - Fully written frames are dequeued; the first partially written frame (if
 *    any) has its length prefix patched in-place to reflect the remaining
 *    unsent portion and stays at the head of the ring for the next flush.
 *
 * Callers wishing to guarantee the ring becomes empty must loop calling
 * ::perf_batcher_flush() until ::perf_batcher_is_empty() returns true (and
 * handle transient EAGAIN / EINTR from `writev`).
 *
 * @param b   Batcher handle.
 * @param fd  Destination file descriptor (normally a socket or pipe). Should
 *            be ready for writing; if non-blocking a partial write is normal.
 * @return 0 on success (even if only some frames were written); -1 on error
 *         (errno from `writev` or EINVAL for bad arguments).
 */
int perf_batcher_flush(po_perf_batcher_t *b, int fd) __nonnull((1));

/**
 * @brief Block until at least one item is available then drain up to
 *        @c batch_size items.
 *
 * The call performs a blocking `read(2)` on the internal eventfd (respecting
 * its semaphore semantics). Once awakened it dequeues up to @c batch_size
 * pointer items into @p out in FIFO order. The number returned can be less
 * than @c batch_size if the producer queue drained.
 *
 * @param b   Batcher handle.
 * @param out Caller-provided array with capacity >= @c batch_size.
 * @return Number of items dequeued (>=1) or -1 on error (errno set: EBADF if
 *         destroyed concurrently, EINTR if interrupted, EINVAL invalid state).
 */
ssize_t perf_batcher_next(po_perf_batcher_t *restrict b, void **restrict out) __nonnull((1, 2));

/**
 * @brief Fast non-blocking emptiness test (advisory only under concurrency).
 *
 * @param b Batcher handle.
 * @return true if the underlying ring buffer count is 0 at the instant of the
 *         call; false otherwise. Races with producer enqueue operations may
 *         make the result immediately stale.
 */
bool perf_batcher_is_empty(const po_perf_batcher_t *b) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // _PERF_BATCHER_H
