#include "net/net.h"
#include "net/socket.h"
#include "net/protocol.h"
#include "perf/zerocopy.h"
#include "perf/batcher.h"
#include "utils/errors.h"
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdio.h>


static perf_zcpool_t     *g_frame_pool = NULL;
static framing_encoder_t *g_encoder    = NULL;
static size_t             g_ring_depth = 0;

static inline int _modify_epoll_events(po_conn_t *c, uint32_t events) {
    return po_modify_events(c, events);
}

int po_init(
    size_t   max_events,
    size_t   buf_count,
    uint32_t buf_size,
    size_t   ring_depth
) {
    if (sock_init(max_events) < 0)
        return -1;

    g_frame_pool = perf_zcpool_create(buf_count, buf_size);
    if (!g_frame_pool) {
        sock_shutdown();
        return -1;
    }

    g_encoder = framing_encoder_new(g_frame_pool, buf_size - 8);
    if (!g_encoder) {
        perf_zcpool_destroy(&g_frame_pool);
        sock_shutdown();
        return -1;
    }

    g_ring_depth = ring_depth;
    return 0;
}

void po_shutdown(void) {
    if (g_encoder) {
        framing_encoder_free(&g_encoder);
        g_encoder = NULL;
    }

    if (g_frame_pool) {
        perf_zcpool_destroy(&g_frame_pool);
        g_frame_pool = NULL;
    }

    sock_shutdown();
}

static void _net_event_cb(po_conn_t *c, uint32_t events, void *arg) {
    (void)arg; // unused

    if (events & EPOLLIN) {
        void *buf = perf_zcpool_acquire(g_frame_pool);
        if (buf) {
            ssize_t n = recv(c->fd, buf, perf_zcpool_bufsize(g_frame_pool), MSG_DONTWAIT);
            if (n > 0)
                framing_decoder_feed(c->decoder, buf);
            else
                perf_zcpool_release(g_frame_pool, buf);
        }
    }

    if (events & EPOLLOUT) {
        bool had_pending = !perf_batcher_is_empty(c->batcher);
        perf_batcher_flush(c->batcher, c->fd);

        if (perf_batcher_is_empty(c->batcher)) {
            if (_modify_epoll_events(c, EPOLLIN) < 0)
                perror("DEBUG: Failed to modify epoll events");
        }
        else if (!had_pending) {
            // Still has pending writes, keep EPOLLOUT
            if (_modify_epoll_events(c, EPOLLIN | EPOLLOUT) < 0)
                perror("DEBUG: Failed to keep EPOLLOUT");

            printf("DEBUG: Pending writes remain, keeping EPOLLOUT\n");
        }
    }

    if (events & EPOLLERR) {
        // Handle error, e.g. close connection
        sock_close(&c);
        return;
    }
}

static po_conn_t *_wrap_conn(po_conn_t *raw) {
    if (!raw)
        return NULL;

    raw->decoder = framing_decoder_new(g_ring_depth, g_frame_pool);
    raw->outq    = perf_ringbuf_create(64); // outgoing frames queue
    raw->batcher = perf_batcher_create(raw->outq, 64); // batch size 64
    if (!raw->decoder || !raw->outq || !raw->batcher)
        goto fail;

    po_set_event_cb(raw, _net_event_cb, NULL); // register event callback
    return raw;

fail:
    sock_close(&raw);
    po_close(&raw);
    return NULL;
}


po_conn_t *po_listen(const char *path, uint16_t port) {
    po_conn_t *raw = sock_listen(path, port);
    if (!raw)
        return NULL;

    return _wrap_conn(raw);
}

po_conn_t *po_connect(const char *path, uint16_t port) {
    po_conn_t *raw = sock_connect(path, port);
    if (!raw)
        return NULL;

    return _wrap_conn(raw);
}

int po_accept(const po_conn_t *listener, po_conn_t **out) {
    po_conn_t *raw = NULL;
    int rc = sock_accept(listener, &raw);
    if (rc < 0)
        return rc;

    *out = _wrap_conn(raw);
    if (!*out)
        return -1;

    return 0;
}

void po_close(po_conn_t **c) {
    if (!*c)
        return;

    if ((*c)->decoder)
        framing_decoder_free(&(*c)->decoder);

    if ((*c)->batcher)
        perf_batcher_destroy(&(*c)->batcher);

    sock_close(c);
    *c = NULL;
}

int po_send_msg(
    po_conn_t *conn,
    uint8_t msg_type,
    uint8_t flags,
    const void *payload,
    uint32_t payload_len
) {
    void *frame;
    uint32_t frame_len;
    if (po_proto_encode(
            g_encoder,
            msg_type,
            flags,
            payload,
            payload_len,
            &frame, &frame_len
        ) < 0)
        return -1;

    if (perf_batcher_enqueue(conn->batcher, frame) < 0) {
        perf_zcpool_release(g_frame_pool, frame);
        errno = EAGAIN;
        return -1;
    }

    // Enable EPOLLOUT to flush
    if (_modify_epoll_events(conn, EPOLLIN | EPOLLOUT) < 0) {
        perf_zcpool_release(g_frame_pool, frame);
        return -1;
    }

    if (perf_batcher_flush(conn->batcher, conn->fd) < 0) {
        // If flush fails, we assume the socket is not writable
        // and will handle it in the event callback
        _modify_epoll_events(conn, EPOLLIN | EPOLLOUT);
        return -1;
    }

    return 0;
}

int po_recv_msg(
    po_conn_t   *conn,
    uint8_t     *msg_type,
    uint8_t     *flags,
    void       **payload,
    uint32_t    *payload_len
) {
    // Just try to decode the next complete frame
    return po_proto_decode(
        conn->decoder,
        msg_type,
        flags,
        payload,
        payload_len
    );
}
