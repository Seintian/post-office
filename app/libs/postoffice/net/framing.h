#ifndef _NET_FRAMING_H
#define _NET_FRAMING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "perf/ringbuf.h"
#include "perf/zerocopy.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _framing_encoder_t {
    perf_zcpool_t *pool;
    uint32_t       max_payload;
};

struct _framing_decoder_t {
    perf_ringbuf_t *rb;    ///< holds void* pointers to full frames
    perf_zcpool_t  *pool;  ///< zcpool for both encoder and release
};

typedef struct _framing_encoder_t framing_encoder_t;
typedef struct _framing_decoder_t framing_decoder_t;

/**
 * @brief Create a framing encoder.
 * @param pool         Zero‑copy pool for frame buffers.
 * @param max_payload  Maximum payload bytes.
 * @return encoder or NULL+errno
 */
framing_encoder_t *framing_encoder_new(
    perf_zcpool_t *pool,
    uint32_t       max_payload
) __nonnull((1));

/**
 * @brief Encode one payload to a single zcpool buffer: [4‑byte BE len][payload].
 * @return 0 or -1+errno.  On success *out_frame,*out_len.
 */
int framing_encode(
    framing_encoder_t *enc,
    const void        *payload,
    uint32_t           payload_len,
    void             **out_frame,
    uint32_t          *out_len
) __nonnull((1, 2, 4, 5));

/** Destroy encoder. */
void framing_encoder_free(framing_encoder_t **enc) __nonnull((1));


/**
 * @brief Create a framing decoder whose ring can hold up to `depth` frames.
 * @param depth  Number of frame pointers capacity (power‐of‐two).
 * @return decoder or NULL+errno.
 */
framing_decoder_t *framing_decoder_new(
    size_t         depth,
    perf_zcpool_t *pool
) __nonnull((2));

/**
 * @brief Feed one complete frame buffer (as returned by encoder or recv).
 * @return 0 or -1+errno=ENOSPC if queue full.
 */
int framing_decoder_feed(
    framing_decoder_t *dec,
    void              *frame
) __nonnull((1, 2));

/**
 * @brief Pop next frame's payload out of queue.
 *        Strips off its 4‑byte length header *in place*; returns the pointer
 *        (which still owns the entire buffer) and payload length.
 * @return 1 if a frame was returned, 0 if queue empty, -1 on error.
 */
int framing_decoder_next(
    framing_decoder_t *dec,
    void             **out_payload,
    uint32_t          *out_len
) __nonnull((1, 2, 3));

/** Destroy decoder (frees its ring only; frames must be released by caller). */
void framing_decoder_free(framing_decoder_t **dec) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // _NET_FRAMING_H
