#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "perf/batcher.h"
#include "perf/ringbuf.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <sys/uio.h>
#include <stdbool.h>
#include <stdio.h>


struct perf_batcher {
    perf_ringbuf_t  *rb;
    int              efd;
    size_t           batch_size;
};

perf_batcher_t *perf_batcher_create(perf_ringbuf_t *rb, size_t batch_size) {
    if (batch_size == 0) {
        errno = EINVAL;
        return NULL;
    }

    perf_batcher_t *b = malloc(sizeof(*b));
    if (!b)
        return NULL;

    b->rb = rb;
    b->batch_size = batch_size;

    // Create an eventfd for producer->consumer signaling
    b->efd = eventfd(0, EFD_CLOEXEC | EFD_SEMAPHORE);
    if (b->efd < 0) {
        free(b);
        return NULL;
    }

    return b;
}

void perf_batcher_destroy(perf_batcher_t **b) {
    if (!*b)
        return;

    perf_batcher_t *batcher = *b;

    if (batcher->efd >= 0) {
        close(batcher->efd);
        batcher->efd = -1;
    }

    // The user must free the ring buffer separately

    free(batcher);
    *b = NULL;
}

int perf_batcher_enqueue(perf_batcher_t *b, void *item) {
    if (b->rb == NULL || b->efd < 0) {
        errno = EINVAL;
        return -1;
    }

    // Enqueue into ring
    if (perf_ringbuf_enqueue(b->rb, item) < 0) {
        errno = EAGAIN;  // full
        return -1;
    }

    // Signal consumer: increment counter by 1
    uint64_t inc = 1;
    if (write(b->efd, &inc, sizeof(inc)) != sizeof(inc)) {
        // if this fails, we consider it an internal error
        errno = EIO;
        return -1;
    }

    return 0;
}

int perf_batcher_flush(perf_batcher_t *b, int fd) {
    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }

    size_t count = perf_ringbuf_count(b->rb);
    if (count == 0)
        return 0;

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
        iov[i].iov_len  = len;
    }

    ssize_t written = writev(fd, iov, (int)i);
    printf("DEBUG: writev returned %zd\n", written);
    if (written < 0)
        return -1;

    // Remove frames that were fully written
    ssize_t consumed = written;
    for (size_t j = 0; j < i; j++) {
        uint32_t len = *(uint32_t *)iov[j].iov_base;

        if (consumed >= (ssize_t)len) {
            perf_ringbuf_dequeue(b->rb, NULL); // drop from queue
            consumed -= len;
        }
        else {
            // Partial write - adjust the frame pointer
            char *base = iov[j].iov_base;
            base += consumed;
            *(uint32_t *)base = len - (uint32_t)consumed; // update remaining size
            break;
        }
    }

    return 0;
}

ssize_t perf_batcher_next(perf_batcher_t *b, void **out) {
    if (b->rb == NULL || b->efd < 0) {
        errno = EINVAL;
        return -1;
    }

    // Block until at least one event arrives
    uint64_t cnt;
    if (read(b->efd, &cnt, sizeof(cnt)) != sizeof(cnt))
        return -1;  // efd closed or error

    // Now drain up to batch_size items
    size_t n = 0;
    for (; n < b->batch_size; n++) {
        void *item;
        if (perf_ringbuf_dequeue(b->rb, &item) < 0)
            break;  // empty

        out[n] = item;
    }

    return (ssize_t)n;
}

bool perf_batcher_is_empty(const perf_batcher_t *b) {
    if (!b->rb)
        return true;

    return perf_ringbuf_count(b->rb) == 0;
}
