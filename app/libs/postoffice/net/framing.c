#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "net/framing.h"
#include "utils/errors.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>  // htonl/ntohl

// -----------------------------------------------------------------------------
// framing_encoder
// -----------------------------------------------------------------------------

framing_encoder_t *framing_encoder_new(
    perf_zcpool_t *pool,
    uint32_t       max_payload
) {
    if (max_payload == 0) {
        errno = NET_EINVAL;
        return NULL;
    }

    framing_encoder_t *e = malloc(sizeof(*e));
    if (!e)
        return NULL;

    e->pool        = pool;
    e->max_payload = max_payload;
    return e;
}

int framing_encode(
    framing_encoder_t *enc,
    const void        *payload,
    uint32_t           payload_len,
    void             **out_frame,
    uint32_t          *out_len
) {
    if (payload_len > enc->max_payload) {
        errno = NET_EMSGSIZE;  // payload too big
        return -1;
    }

    // grab one big buffer: 4 B header + payload
    void *buf = perf_zcpool_acquire(enc->pool);
    if (!buf) {
        // errno == EAGAIN from pool
        errno = NET_EAGAIN;
        return -1;
    }

    uint32_t be_len = htonl(payload_len);
    memcpy(buf, &be_len, 4);
    if (payload_len)
        memcpy((char*)buf + 4, payload, payload_len);

    *out_frame = buf;
    *out_len   = payload_len + 4;
    return 0;
}

void framing_encoder_free(framing_encoder_t **penc) {
    if (!*penc)
        return;

    free(*penc);
    *penc = NULL;
}


// -----------------------------------------------------------------------------
// framing_decoder
// -----------------------------------------------------------------------------

framing_decoder_t *framing_decoder_new(
    size_t         depth,
    perf_zcpool_t *pool
) {
    if ((depth & (depth - 1)) != 0 || depth < 2) {
        errno = NET_EINVAL;
        return NULL;
    }

    framing_decoder_t *d = malloc(sizeof(*d));
    if (!d)
        return NULL;

    d->rb = perf_ringbuf_create(depth);
    if (!d->rb) {
        free(d);
        return NULL;
    }

    d->pool = pool;
    return d;
}

int framing_decoder_feed(
    framing_decoder_t *dec,
    void              *frame
) {
    // enqueue the full buffer pointer
    if (perf_ringbuf_enqueue(dec->rb, frame) < 0) {
        // ring full ⇒ ENOSPC
        errno = NET_ENOSPC;
        return -1;
    }

    return 0;
}

int framing_decoder_next(
    framing_decoder_t *dec,
    void             **out_payload,
    uint32_t          *out_len
) {
    void *frame;
    int rc = perf_ringbuf_dequeue(dec->rb, &frame);
    if (rc < 0) {
        // empty
        return 0;
    }

    // frame is [4B BE length][payload…]
    uint32_t be;
    memcpy(&be, frame, 4);
    uint32_t payload_len = ntohl(be);

    *out_payload = (char*)frame + 4;
    *out_len     = payload_len;
    return 1;
}

void framing_decoder_free(framing_decoder_t **pdec) {
    if (!*pdec)
        return;

    framing_decoder_t *d = *pdec;
    perf_ringbuf_destroy(&d->rb);
    free(d);
    *pdec = NULL;
}
