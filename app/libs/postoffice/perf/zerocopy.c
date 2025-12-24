#include "perf/zerocopy.h"

#include <errno.h>
#include <linux/mman.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "perf/ringbuf.h"
#include "utils/errors.h"
#include "metrics/metrics.h"
#include <postoffice/log/logger.h>

// Include pthread
#include <pthread.h>

struct perf_zcpool {
    void *base; // start of mapped region
    size_t buf_size;
    size_t buf_count;
    po_perf_ringbuf_t *freeq; // ring buffer of free pointers
    perf_zcpool_flags_t flags;
    pthread_mutex_t lock;
};

size_t perf_zcpool_bufsize(const perf_zcpool_t *pool) {
    return pool->buf_size;
}

perf_zcpool_t *perf_zcpool_create(size_t buf_count, size_t buf_size, perf_zcpool_flags_t flags) {
    if (buf_count < 1 || buf_size == 0 || buf_size > (2UL << 20)) {
        errno = EINVAL;  // Use standard EINVAL instead of ZCP_EINVAL
        return NULL;
    }

    // Compute total size, rounded up to multiple of 2 MiB
    size_t region_size = buf_count * buf_size;
    size_t page_2mb = 2UL << 20;
    size_t aligned = (region_size + page_2mb - 1) & ~(page_2mb - 1);

    // Try mmap with 2 MiB huge pages
    void *base = mmap(NULL, aligned, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | (21 << MAP_HUGE_SHIFT), -1, 0);
    if (base == MAP_FAILED) {
        // fallback to normal pages
        base = mmap(NULL, aligned, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) {
            errno = ZCP_EMMAP;
            return NULL;
        }
    }

    perf_zcpool_t *p = malloc(sizeof(*p));
    if (!p) {
        munmap(base, aligned); // if unmap fails, we can't do much here
        return NULL;
    }

    p->base = base;
    p->buf_size = buf_size;
    p->buf_count = buf_count;
    p->flags = flags;
    // We do NOT use PERF_RINGBUF_METRICS for internal freeq unless ZCP_METRICS is set
    // AND even then, the pool call usually implies the ring call.
    // However, if we enable both, we get double metrics for enqueue/dequeue vs acquire/release.
    // Let's NOT enable metrics on the internal ringbuf to keep it cleaner.
    // Use buf_count + 1 to ensure we can hold all buf_count items (ringbuf full is N-1)
    p->freeq = perf_ringbuf_create(buf_count * 2, PERF_RINGBUF_NOFLAGS);
    if (!p->freeq) {
        munmap(base, aligned);
        free(p);
        return NULL;
    }

    // Initialize mutex
    if (pthread_mutex_init(&p->lock, NULL) != 0) {
        perf_ringbuf_destroy(&p->freeq);
        munmap(base, aligned);
        free(p);
        return NULL;
    }

    // Populate free list with each buffer pointer
    int enqueue_fails = 0;
    for (size_t i = 0; i < buf_count; i++) {
        void *ptr = (char *)base + i * buf_size;
        if (perf_ringbuf_enqueue(p->freeq, ptr) != 0) {
            enqueue_fails++;
        }
    }
    
    if (enqueue_fails > 0 || perf_ringbuf_count(p->freeq) != buf_count) {
        LOG_ERROR("zcpool_create: enqueued %zu/%zu buffers. Fails: %d", 
            perf_ringbuf_count(p->freeq), buf_count, enqueue_fails);
    } else {
        LOG_DEBUG("zcpool_create: successfully created pool with %zu buffers", buf_count);
    }

    if (flags & PERF_ZCPOOL_METRICS) {
        PO_METRIC_COUNTER_CREATE("zcpool.create");
        PO_METRIC_COUNTER_CREATE("zcpool.destroy");
        PO_METRIC_COUNTER_CREATE("zcpool.acquire");
        PO_METRIC_COUNTER_CREATE("zcpool.release");
        PO_METRIC_COUNTER_CREATE("zcpool.exhausted");
        PO_METRIC_COUNTER_INC("zcpool.create");
    }

    return p;
}

void perf_zcpool_destroy(perf_zcpool_t **p) {
    if (!*p)
        return;

    perf_zcpool_t *pool = *p;

    if (pool->flags & PERF_ZCPOOL_METRICS)
        PO_METRIC_COUNTER_INC("zcpool.destroy");

    pthread_mutex_destroy(&pool->lock);

    // free ring and unmap region
    if (pool->freeq)
        perf_ringbuf_destroy(&pool->freeq);

    if (pool->base) {
        size_t region_size = pool->buf_count * pool->buf_size;
        size_t page_2mb = 2UL << 20;
        size_t aligned = (region_size + page_2mb - 1) & ~(page_2mb - 1);
        munmap(pool->base, aligned);
    }

    free(pool);
    *p = NULL;
}

void *perf_zcpool_acquire(perf_zcpool_t *p) {
    if (!p->freeq) {
        errno = EINVAL;  // Use standard EINVAL instead of ZCP_EINVAL
        return NULL;
    }

    pthread_mutex_lock(&p->lock);
    void *buf = NULL;
    if (perf_ringbuf_dequeue(p->freeq, &buf) < 0) {
        pthread_mutex_unlock(&p->lock);
        if (p->flags & PERF_ZCPOOL_METRICS)
            PO_METRIC_COUNTER_INC("zcpool.exhausted");
        errno = EAGAIN;
        return NULL;
    }
    pthread_mutex_unlock(&p->lock);

    if (p->flags & PERF_ZCPOOL_METRICS)
        PO_METRIC_COUNTER_INC("zcpool.acquire");

    return buf;
}

void perf_zcpool_release(perf_zcpool_t *p, void *buffer) {
    if (!p->freeq) {
        errno = EINVAL;  // Use standard EINVAL instead of ZCP_EINVAL
        return;
    }

    uintptr_t start = (uintptr_t)p->base;
    uintptr_t end = start + p->buf_count * p->buf_size;
    uintptr_t ptr = (uintptr_t)buffer;
    // only release if pointer is exactly one of our buffers
    if (ptr < start || ptr >= end || ((ptr - start) % p->buf_size) != 0)
        return;

    pthread_mutex_lock(&p->lock);
    perf_ringbuf_enqueue(p->freeq, buffer);
    pthread_mutex_unlock(&p->lock);

    if (p->flags & PERF_ZCPOOL_METRICS)
        PO_METRIC_COUNTER_INC("zcpool.release");
}

size_t perf_zcpool_freecount(const perf_zcpool_t *p) {
    if (!p || !p->freeq) {
        errno = EINVAL;
        return 0;
    }

    return perf_ringbuf_count(p->freeq);
}
