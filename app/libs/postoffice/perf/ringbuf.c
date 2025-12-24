#include "perf/ringbuf.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include <stdalign.h>
#include <stdbool.h>

#include "metrics/metrics.h"

/*
    MPMC Sequenced Ring Buffer (based on Dmitry Vyukov's algorithm)
    Each slot has a sequence number to synchronize producers and consumers.
*/

typedef struct {
    atomic_size_t seq;
    void *item;
} slot_t;

struct perf_ringbuf {
    alignas(64) atomic_size_t head; // read index
    alignas(64) atomic_size_t tail; // write index
    size_t cap;                     // must be power‐of‐two
    size_t mask;                    // cap - 1
    slot_t *slots;                  // array[cap]
    perf_ringbuf_flags_t flags;
};

static size_t _cacheline = 64;

void perf_ringbuf_set_cacheline(size_t cacheline_bytes) {
    if (cacheline_bytes && !(cacheline_bytes & (cacheline_bytes - 1)))
        _cacheline = cacheline_bytes;
    else
        _cacheline = 64; // default
}

po_perf_ringbuf_t *perf_ringbuf_create(size_t capacity, perf_ringbuf_flags_t flags) {
    if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
        errno = EINVAL;
        return NULL;
    }

    po_perf_ringbuf_t *rb = calloc(1, sizeof(*rb));
    if (!rb)
        return NULL;

    rb->cap = capacity;
    rb->mask = capacity - 1;
    rb->flags = flags;
    rb->slots = calloc(capacity, sizeof(slot_t));
    if (!rb->slots) {
        free(rb);
        return NULL;
    }

    for (size_t i = 0; i < capacity; i++) {
        atomic_init(&rb->slots[i].seq, i);
    }

    atomic_init(&rb->head, 0);
    atomic_init(&rb->tail, 0);

    if (flags & PERF_RINGBUF_METRICS) {
        PO_METRIC_COUNTER_CREATE("ringbuf.create");
        PO_METRIC_COUNTER_INC("ringbuf.create");
        PO_METRIC_COUNTER_CREATE("ringbuf.enqueue");
        PO_METRIC_COUNTER_CREATE("ringbuf.dequeue");
        PO_METRIC_COUNTER_CREATE("ringbuf.full");
    }

    return rb;
}

void perf_ringbuf_destroy(po_perf_ringbuf_t **prb) {
    if (!*prb || !prb)
        return;

    po_perf_ringbuf_t *rb = *prb;
    if (rb->flags & PERF_RINGBUF_METRICS) {
        PO_METRIC_COUNTER_INC("ringbuf.destroy");
    }

    free(rb->slots);
    free(rb);
    *prb = NULL;
}

int perf_ringbuf_enqueue(po_perf_ringbuf_t *rb, void *item) {
    slot_t *slot;
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    
    for (;;) {
        slot = &rb->slots[tail & rb->mask];
        size_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)tail;

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&rb->tail, &tail, tail + 1,
                                                     memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            if (rb->flags & PERF_RINGBUF_METRICS)
                PO_METRIC_COUNTER_INC("ringbuf.full");
            errno = EAGAIN;
            return -1;
        } else {
            tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
        }
    }

    slot->item = item;
    atomic_store_explicit(&slot->seq, tail + 1, memory_order_release);

    if (rb->flags & PERF_RINGBUF_METRICS)
        PO_METRIC_COUNTER_INC("ringbuf.enqueue");

    return 0;
}

int perf_ringbuf_dequeue(po_perf_ringbuf_t *rb, void **out) {
    slot_t *slot;
    size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);

    for (;;) {
        slot = &rb->slots[head & rb->mask];
        size_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(head + 1);

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&rb->head, &head, head + 1,
                                                     memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            return -1; // Empty
        } else {
            head = atomic_load_explicit(&rb->head, memory_order_relaxed);
        }
    }

    if (out) *out = slot->item;
    atomic_store_explicit(&slot->seq, head + rb->mask + 1, memory_order_release);

    if (rb->flags & PERF_RINGBUF_METRICS)
        PO_METRIC_COUNTER_INC("ringbuf.dequeue");

    return 0;
}

size_t perf_ringbuf_count(const po_perf_ringbuf_t *rb) {
    size_t h = atomic_load_explicit(&rb->head, memory_order_relaxed);
    size_t t = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    return (t > h) ? (t - h) : 0;
}

int perf_ringbuf_peek(const po_perf_ringbuf_t *rb, void **out) {
    size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    slot_t *slot = &rb->slots[head & rb->mask];
    size_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
    
    if ((intptr_t)seq - (intptr_t)(head + 1) == 0) {
        if (out) *out = slot->item;
        return 0;
    }
    return -1;
}

int perf_ringbuf_peek_at(const po_perf_ringbuf_t *rb, size_t idx, void **out) {
    size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    size_t target = head + idx;
    slot_t *slot = &rb->slots[target & rb->mask];
    size_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);

    if ((intptr_t)seq - (intptr_t)(target + 1) == 0) {
        if (out) *out = slot->item;
        return 0;
    }
    return -1;
}

int perf_ringbuf_advance(po_perf_ringbuf_t *rb, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (perf_ringbuf_dequeue(rb, NULL) != 0) return -1;
    }
    return 0;
}
