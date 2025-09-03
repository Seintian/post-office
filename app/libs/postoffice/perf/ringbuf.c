#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "perf/ringbuf.h"
#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

/*
    head (read index)  and  tail (write index)  are each allocated
    on their own cache‐line so there's no false sharing.
*/
struct perf_ringbuf {
    atomic_size_t *head; // read index
    atomic_size_t *tail; // write index
    size_t cap;          // must be power‐of‐two
    size_t mask;         // cap - 1
    void **slots;        // array[cap]
};

static size_t _cacheline = 64;

void perf_ringbuf_set_cacheline(size_t cacheline_bytes) {
    if (cacheline_bytes && !(cacheline_bytes & (cacheline_bytes - 1)))
        _cacheline = cacheline_bytes;
    else
        _cacheline = 64; // default
}

perf_ringbuf_t *perf_ringbuf_create(size_t capacity) {
    if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
        errno = EINVAL;
        return NULL;
    }

    perf_ringbuf_t *rb = calloc(1, sizeof(*rb));
    if (!rb)
        return NULL;

    rb->cap = capacity;
    rb->mask = capacity - 1;
    rb->slots = calloc(capacity, sizeof(void *));
    if (!rb->slots) {
        free(rb);
        return NULL;
    }

    // align head and tail on separate cache lines
    if (posix_memalign((void **)&rb->head, _cacheline, sizeof(*rb->head)) != 0 ||
        posix_memalign((void **)&rb->tail, _cacheline, sizeof(*rb->tail)) != 0) {
        free(rb->slots);
        free(rb);
        return NULL;
    }

    atomic_init(rb->head, 0);
    atomic_init(rb->tail, 0);
    return rb;
}

void perf_ringbuf_destroy(perf_ringbuf_t **prb) {
    if (!*prb)
        return;

    perf_ringbuf_t *rb = *prb;
    free(rb->slots);
    free(rb->head);
    free(rb->tail);
    free(rb);
    *prb = NULL;
}

int perf_ringbuf_enqueue(perf_ringbuf_t *rb, void *item) {
    size_t head = atomic_load_explicit(rb->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(rb->tail, memory_order_relaxed);

    // full if writing would catch the reader
    if (((tail + 1) & rb->mask) == (head & rb->mask))
        return -1;

    rb->slots[tail & rb->mask] = item;
    atomic_store_explicit(rb->tail, tail + 1, memory_order_release);
    return 0;
}

int perf_ringbuf_dequeue(perf_ringbuf_t *rb, void **out) {
    size_t head = atomic_load_explicit(rb->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(rb->tail, memory_order_acquire);

    // empty if nothing written
    if ((head & rb->mask) == (tail & rb->mask))
        return -1;

    if (out)
        *out = rb->slots[head & rb->mask];

    atomic_store_explicit(rb->head, head + 1, memory_order_release);
    return 0;
}

size_t perf_ringbuf_count(const perf_ringbuf_t *rb) {
    size_t head = atomic_load_explicit(rb->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(rb->tail, memory_order_acquire);
    // how many writes ahead of reads?
    return (tail - head);
}

int perf_ringbuf_peek(const perf_ringbuf_t *rb, void **out) {
    size_t head = atomic_load_explicit(rb->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(rb->tail, memory_order_acquire);

    if (head == tail) // empty?
        return -1;

    if (out)
        *out = rb->slots[head & rb->mask];

    return 0;
}

int perf_ringbuf_peek_at(const perf_ringbuf_t *rb, size_t idx, void **out) {
    size_t cnt = perf_ringbuf_count(rb);
    if (idx >= cnt)
        return -1;

    size_t head = atomic_load_explicit(rb->head, memory_order_acquire);
    if (out)
        *out = rb->slots[(head + idx) & rb->mask];

    return 0;
}

int perf_ringbuf_advance(perf_ringbuf_t *rb, size_t n) {
    size_t cnt = perf_ringbuf_count(rb);
    if (n > cnt)
        return -1;

    atomic_fetch_add_explicit(rb->head, n, memory_order_release);
    return 0;
}
