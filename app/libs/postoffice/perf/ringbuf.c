#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "perf/ringbuf.h"
#include <stdlib.h>
#include <errno.h>
#include <stdatomic.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <threads.h>


/*
    We allocate head and tail each in their own cache‐line‐sized block,
    so they never share a cache line.
*/

struct _perf_ringbuf_t {
    atomic_size_t *head;   // pointer to a cache-line-aligned slot
    atomic_size_t *tail;   // likewise
    size_t         cap;    // must be power‐of‐two
    size_t         mask;   // cap - 1
    void         **slots;  // array[cap]
};

static size_t _cacheline = 64;

void perf_ringbuf_set_cacheline(size_t cacheline_bytes) {
    if (cacheline_bytes && (cacheline_bytes & (cacheline_bytes - 1)) == 0)
        _cacheline = cacheline_bytes;
    else
        _cacheline = 64;
}

perf_ringbuf_t *perf_ringbuf_create(size_t capacity) {
    if ((capacity & (capacity - 1)) != 0) {
        errno = EINVAL;
        return NULL;
    }

    perf_ringbuf_t *rb = malloc(sizeof(*rb));
    if (!rb) 
        return NULL;
    memset(rb, 0, sizeof(*rb));

    rb->cap   = capacity;
    rb->mask  = capacity - 1;
    rb->slots = calloc(capacity, sizeof(void*));
    if (!rb->slots) {
        free(rb);
        return NULL;
    }

    // allocate head and tail each on its own cache line
    if (posix_memalign((void**)&rb->head, _cacheline, _cacheline) ||
        posix_memalign((void**)&rb->tail, _cacheline, _cacheline)) {
        free(rb->slots);
        free(rb);
        return NULL;
    }

    atomic_init(rb->head, 0);
    atomic_init(rb->tail, 0);
    return rb;
}

void perf_ringbuf_destroy(perf_ringbuf_t **rb) {
    if (!rb || !*rb)
        return;

    perf_ringbuf_t *ringbuf = *rb;

    if (ringbuf->slots) {
        free(ringbuf->slots);
        ringbuf->slots = NULL;
    }

    if (ringbuf->head) {
        free(ringbuf->head);
        ringbuf->head = NULL;
    }

    if (ringbuf->tail) {
        free(ringbuf->tail);
        ringbuf->tail = NULL;
    }

    free(ringbuf);
    *rb = NULL;
}

int perf_ringbuf_enqueue(perf_ringbuf_t *rb, void *item) {
    size_t h = atomic_load_explicit(rb->head, memory_order_relaxed);
    size_t t = atomic_load_explicit(rb->tail, memory_order_acquire);

    if (((h + 1) & rb->mask) == t)
        return -1;  // full

    rb->slots[h] = item;
    atomic_store_explicit(
        rb->head,
        (h + 1) & rb->mask,
        memory_order_release
    );
    return 0;
}

int perf_ringbuf_dequeue(perf_ringbuf_t *rb, void **out) {
    size_t t = atomic_load_explicit(rb->tail, memory_order_relaxed);
    size_t h = atomic_load_explicit(rb->head, memory_order_acquire);

    if (t == h)
        return -1;  // empty

    *out = rb->slots[t];
    atomic_store_explicit(
        rb->tail,
        (t + 1) & rb->mask,
        memory_order_release
    );
    return 0;
}

size_t perf_ringbuf_count(const perf_ringbuf_t *rb) {
    size_t h = atomic_load_explicit(rb->head, memory_order_acquire);
    size_t t = atomic_load_explicit(rb->tail, memory_order_acquire);
    if (h >= t)
        return h - t;

    return (h + rb->cap) - t;
}
