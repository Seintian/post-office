#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

struct perf_zcpool {
    void *base; // start of mapped region
    size_t buf_size;
    size_t buf_count;
    po_perf_ringbuf_t *freeq; // ring buffer of free pointers
};

size_t perf_zcpool_bufsize(const perf_zcpool_t *pool) {
    return pool->buf_size;
}

perf_zcpool_t *perf_zcpool_create(size_t buf_count, size_t buf_size) {
    if (buf_count < 1 || buf_size == 0 || buf_size > (2UL << 20)) {
        errno = ZCP_EINVAL;
        return NULL;
    }

    // Compute total size, rounded up to multiple of 2 MiB
    size_t region_size = buf_count * buf_size;
    size_t page_2mb = 2UL << 20;
    size_t aligned = (region_size + page_2mb - 1) & ~(page_2mb - 1);

    // Try mmap with 2 MiB huge pages
    void *base = mmap(
        NULL,
        aligned,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | (21 << MAP_HUGE_SHIFT),
        -1,
        0
    );
    if (base == MAP_FAILED) {
        // fallback to normal pages
        base = mmap(
            NULL,
            aligned,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        );
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
    p->freeq = perf_ringbuf_create(buf_count);
    if (!p->freeq) {
        munmap(base, aligned);
        free(p);
        return NULL;
    }

    // Populate free list with each buffer pointer
    for (size_t i = 0; i < buf_count; i++) {
        void *ptr = (char *)base + i * buf_size;
        perf_ringbuf_enqueue(p->freeq, ptr);
    }

    return p;
}

void perf_zcpool_destroy(perf_zcpool_t **p) {
    if (!*p)
        return;

    perf_zcpool_t *pool = *p;

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
        errno = ZCP_EINVAL;
        return NULL;
    }

    void *buf = NULL;
    if (perf_ringbuf_dequeue(p->freeq, &buf) < 0) {
        errno = ZCP_EAGAIN;
        return NULL;
    }

    return buf;
}

void perf_zcpool_release(perf_zcpool_t *p, void *buffer) {
    if (!p->freeq) {
        errno = ZCP_EINVAL;
        return;
    }

    uintptr_t start = (uintptr_t)p->base;
    uintptr_t end = start + p->buf_count * p->buf_size;
    uintptr_t ptr = (uintptr_t)buffer;
    // only release if pointer is exactly one of our buffers
    if (ptr < start || ptr >= end || ((ptr - start) % p->buf_size) != 0)
        return;

    // Optionally check buffer in range [base, base+count*size)
    perf_ringbuf_enqueue(p->freeq, buffer);
}

size_t perf_zcpool_freecount(const perf_zcpool_t *p) {
    if (!p->freeq) {
        errno = ZCP_EINVAL;
        return 0;
    }

    return perf_ringbuf_count(p->freeq);
}
