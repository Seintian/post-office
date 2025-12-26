#include "perf/batcher.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/uio.h>
#include <unistd.h>

#include "perf/ringbuf.h"
#include "metrics/metrics.h"

struct perf_batcher {
    po_perf_ringbuf_t *rb;
    int efd;
    size_t batch_size;
    perf_batcher_flags_t flags;
};

po_perf_batcher_t *perf_batcher_create(po_perf_ringbuf_t *rb, size_t batch_size, perf_batcher_flags_t flags) {
    if (batch_size == 0) {
        errno = EINVAL;
        return NULL;
    }

    po_perf_batcher_t *b = malloc(sizeof(*b));
    if (!b)
        return NULL;

    b->rb = rb;
    b->batch_size = batch_size;
    b->flags = flags;

    // Create an eventfd for producer->consumer signaling
    b->efd = eventfd(0, EFD_CLOEXEC | EFD_SEMAPHORE);
    if (b->efd < 0) {
        free(b);
        return NULL;
    }

    if (flags & PERF_BATCHER_METRICS) {
        PO_METRIC_COUNTER_CREATE("batcher.create");
        PO_METRIC_COUNTER_INC("batcher.create");
        PO_METRIC_COUNTER_CREATE("batcher.enqueue");
        PO_METRIC_COUNTER_CREATE("batcher.full");
        PO_METRIC_COUNTER_CREATE("batcher.flush");
        PO_METRIC_COUNTER_CREATE("batcher.next");
    }

    return b;
}

void perf_batcher_destroy(po_perf_batcher_t **restrict b) {
    if (!*b)
        return;

    po_perf_batcher_t *batcher = *b;

    if (batcher->efd >= 0) {
        close(batcher->efd);
        batcher->efd = -1;
    }

    // The user must free the ring buffer separately
    
    if (batcher->flags & PERF_BATCHER_METRICS)
        PO_METRIC_COUNTER_INC("batcher.destroy");

    free(batcher);
    *b = NULL;
}

int perf_batcher_enqueue(po_perf_batcher_t *restrict b, void *restrict item) {
    if (b->rb == NULL || b->efd < 0) {
        errno = EINVAL;
        return -1;
    }

    // Enqueue into ring
    if (perf_ringbuf_enqueue(b->rb, item) < 0) {
        if (b->flags & PERF_BATCHER_METRICS)
             PO_METRIC_COUNTER_INC("batcher.full");
        errno = EAGAIN; // full
        return -1;
    }

    // Signal consumer: increment counter by 1
    uint64_t inc = 1;
    ssize_t ret;
    do {
        ret = write(b->efd, &inc, sizeof(inc));
    } while (ret < 0 && errno == EINTR);

    if (ret != sizeof(inc)) {
        // if this fails, we consider it an internal error but item is enqueued
        // return 0 to avoid double-free by caller
        errno = EIO;
        return 0;
    }

    if (b->flags & PERF_BATCHER_METRICS)
        PO_METRIC_COUNTER_INC("batcher.enqueue");

    return 0;
}

int perf_batcher_flush(po_perf_batcher_t *b, int fd) {
    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }

    size_t count = perf_ringbuf_count(b->rb);
    if (count == 0)
        return 0;

    if (b->flags & PERF_BATCHER_METRICS)
        PO_METRIC_COUNTER_INC("batcher.flush");

    if (count > IOV_MAX)
        count = IOV_MAX;

    struct iovec iov[IOV_MAX];

    // Gather frames into iovec
    size_t i = 0;
    for (; i < count; i++) {
        void *frame;
        if (perf_ringbuf_peek_at(b->rb, i, &frame) < 0)
            break;

        uint32_t len = *(uint32_t *)frame; // first 4B = length
        iov[i].iov_base = frame;
        iov[i].iov_len = len;
    }

    ssize_t written = writev(fd, iov, (int)i);
    if (written < 0)
        return -1;

    // Remove frames that were fully written
    ssize_t consumed = written;
    for (size_t j = 0; j < i; j++) {
        uint32_t len = *(uint32_t *)iov[j].iov_base;

        if (consumed >= (ssize_t)len) {
            perf_ringbuf_dequeue(b->rb, NULL); // drop from queue
            consumed -= len;
        } else {
            // Partial write - adjust the frame pointer
            char *base = iov[j].iov_base;
            base += consumed;
            uint32_t remaining = len - (uint32_t)consumed;
            memcpy(base, &remaining, sizeof(remaining));
            break;
        }
    }

    return 0;
}

ssize_t perf_batcher_next(po_perf_batcher_t *restrict b, void **restrict out) {
    if (b->rb == NULL || b->efd < 0) {
        errno = EINVAL;
        return -1;
    }

    // Block until at least one event arrives
    uint64_t cnt;
    if (read(b->efd, &cnt, sizeof(cnt)) != sizeof(cnt))
        return -1; // efd closed or error

    // Now drain up to batch_size items
    size_t n = 0;
    for (; n < b->batch_size; n++) {
        void *item;
        if (perf_ringbuf_dequeue(b->rb, &item) < 0)
            break; // empty

        out[n] = item;
    }

    if (b->flags & PERF_BATCHER_METRICS) {
        PO_METRIC_COUNTER_INC("batcher.next");
        if (n > 0)
            PO_METRIC_COUNTER_ADD("batcher.items", n);
    }

    return (ssize_t)n;
}

bool perf_batcher_is_empty(const po_perf_batcher_t *b) {
    if (!b->rb)
        return true;

    return perf_ringbuf_count(b->rb) == 0;
}
