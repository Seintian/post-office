#include "unity/unity_fixture.h"
#include "net/framing.h"
#include "perf/zerocopy.h"
#include "perf/ringbuf.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>  // htonl/ntohl


TEST_GROUP(Framing);

// common pool & decoder/encoder pointers
static perf_zcpool_t   *pool;
static framing_encoder_t *enc;
static framing_decoder_t *dec;

// small helper
static void release_frame(void *payload) {
    // frame pointer is payload − 4 bytes
    void *frame = (char*)payload - 4;
    perf_zcpool_release(pool, frame);
}

TEST_SETUP(Framing) {
    // create a small pool: 4 buffers × 64 bytes each
    pool = perf_zcpool_create(4, 64);
    TEST_ASSERT_NOT_NULL(pool);
    enc = NULL;
    dec = NULL;
}

TEST_TEAR_DOWN(Framing) {
    if (enc) framing_encoder_free(&enc);
    if (dec) framing_decoder_free(&dec);
    perf_zcpool_destroy(&pool);
}

//------------------------------------------------------------------------
// Encoder tests
//------------------------------------------------------------------------

TEST(Framing, EncoderNew_InvalidParams) {
    // max_payload==0
    TEST_ASSERT_NULL(framing_encoder_new(pool, 0));
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

TEST(Framing, Encode_PayloadTooBig) {
    enc = framing_encoder_new(pool, 8);
    TEST_ASSERT_NOT_NULL(enc);
    char buf[16] = {0};
    void *frame; uint32_t flen;
    TEST_ASSERT_EQUAL_INT(-1, framing_encode(enc, buf, 9, &frame, &flen));
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

TEST(Framing, Encode_Basic) {
    enc = framing_encoder_new(pool, 16);
    TEST_ASSERT_NOT_NULL(enc);

    const char *msg = "hello";
    void *frame;
    uint32_t frame_len;
    TEST_ASSERT_EQUAL_INT(0,
        framing_encode(enc, msg, (uint32_t)strlen(msg), &frame, &frame_len));
    // frame_len == 4 + payload
    TEST_ASSERT_EQUAL_UINT32(4 + 5, frame_len);

    // header is big-endian 5
    uint32_t be;
    memcpy(&be, frame, 4);
    TEST_ASSERT_EQUAL_UINT32(htonl(5), be);

    // payload follows
    TEST_ASSERT_EQUAL_MEMORY(msg, (char*)frame + 4, 5);

    // release back
    perf_zcpool_release(pool, frame);
}

TEST(Framing, Encode_MultipleBuffers) {
    enc = framing_encoder_new(pool, 8);
    TEST_ASSERT_NOT_NULL(enc);

    void *frames[3];
    for (int i = 0; i < 3; i++) {
        char msg[4] = { (char)i,0,0,0 };
        uint32_t fl;
        TEST_ASSERT_EQUAL_INT(0, framing_encode(enc, msg, 1, &frames[i], &fl));
    }
    // pool exhausted now
    void *f5;
    uint32_t fl5;
    TEST_ASSERT_EQUAL_INT(-1,
        framing_encode(enc, "x", 1, &f5, &fl5));
    TEST_ASSERT_NULL(frames[2] == NULL ? NULL : f5);
    // release all 3
    for (int i = 0; i < 3; i++)
        perf_zcpool_release(pool, frames[i]);
}

//------------------------------------------------------------------------
// Decoder tests
//------------------------------------------------------------------------

TEST(Framing, DecoderNew_Valid) {
    dec = framing_decoder_new(4, pool);
    TEST_ASSERT_NOT_NULL(dec);
}

TEST(Framing, DecoderFeedAndNext_Basic) {
    enc = framing_encoder_new(pool, 16);
    dec = framing_decoder_new(4, pool);
    TEST_ASSERT_NOT_NULL(enc);
    TEST_ASSERT_NOT_NULL(dec);

    // make two frames
    void *f1;
    void *f2;
    uint32_t l1;
    uint32_t l2;
    TEST_ASSERT_EQUAL_INT(0, framing_encode(enc, "A", 1, &f1, &l1));
    TEST_ASSERT_EQUAL_INT(0, framing_encode(enc, "BC", 2, &f2, &l2));

    // feed them
    TEST_ASSERT_EQUAL_INT(0, framing_decoder_feed(dec, f1));
    TEST_ASSERT_EQUAL_INT(0, framing_decoder_feed(dec, f2));

    // pop first
    void *p; uint32_t plen;
    TEST_ASSERT_EQUAL_INT(1, framing_decoder_next(dec, &p, &plen));
    TEST_ASSERT_EQUAL_UINT32(1, plen);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("A", p, 1);
    // return buffer
    release_frame(p);

    // pop second
    TEST_ASSERT_EQUAL_INT(1, framing_decoder_next(dec, &p, &plen));
    TEST_ASSERT_EQUAL_UINT32(2, plen);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("BC", p, 2);
    release_frame(p);

    // now empty
    TEST_ASSERT_EQUAL_INT(0, framing_decoder_next(dec, &p, &plen));
}

TEST(Framing, DecoderOrderPreserved) {
    enc = framing_encoder_new(pool, 8);
    dec = framing_decoder_new(8, pool);

    void *f[3];
    uint32_t fl;
    for (int i = 0; i < 3; i++) {
        char tmp[] = { (char)('0'+i), 0 };
        TEST_ASSERT_EQUAL_INT(0, framing_encode(enc, tmp, 1, &f[i], &fl));
        TEST_ASSERT_EQUAL_INT(0, framing_decoder_feed(dec, f[i]));
    }

    for (int i = 0; i < 3; i++) {
        void *p;
        uint32_t plen;
        TEST_ASSERT_EQUAL_INT(1, framing_decoder_next(dec, &p, &plen));
        TEST_ASSERT_EQUAL_UINT32(1, plen);
        char expect = (char)('0'+i);
        TEST_ASSERT_EQUAL_CHAR(expect, ((char*)p)[0]);
        release_frame(p);
    }
}

TEST(Framing, DecoderQueueFull) {
    enc = framing_encoder_new(pool, 8);
    // make depth=2 => can hold only 1 frame (we consider ring of size N holds N-1 items)
    dec = framing_decoder_new(2, pool);
    TEST_ASSERT_NOT_NULL(dec);

    void *f1;
    uint32_t fl;
    TEST_ASSERT_EQUAL_INT(0, framing_encode(enc, "x", 1, &f1, &fl));
    TEST_ASSERT_EQUAL_INT(0, framing_decoder_feed(dec, f1));

    TEST_ASSERT_EQUAL_INT(-1, framing_decoder_feed(dec, f1)); // full
    // return leftover
    release_frame((char*)f1);
}

TEST(Framing, CombinedEncodeFeedNextRelease) {
    enc = framing_encoder_new(pool, 16);
    dec = framing_decoder_new(4, pool);

    // encode+feed in one go
    const char *msg = "ZCQ";
    void *frm; uint32_t fl;
    TEST_ASSERT_EQUAL_INT(0, framing_encode(enc, msg, 3, &frm, &fl));
    TEST_ASSERT_EQUAL_INT(0, framing_decoder_feed(dec, frm));

    // next
    void *payload; uint32_t plen;
    TEST_ASSERT_EQUAL_INT(1, framing_decoder_next(dec, &payload, &plen));
    TEST_ASSERT_EQUAL_UINT32(3, plen);
    TEST_ASSERT_EQUAL_MEMORY(msg, payload, 3);

    // cleanup
    release_frame(payload);
}

TEST_GROUP_RUNNER(Framing) {
    RUN_TEST_CASE(Framing, EncoderNew_InvalidParams);
    RUN_TEST_CASE(Framing, Encode_PayloadTooBig);
    RUN_TEST_CASE(Framing, Encode_Basic);
    RUN_TEST_CASE(Framing, Encode_MultipleBuffers);

    RUN_TEST_CASE(Framing, DecoderNew_Valid);
    RUN_TEST_CASE(Framing, DecoderFeedAndNext_Basic);
    RUN_TEST_CASE(Framing, DecoderOrderPreserved);
    RUN_TEST_CASE(Framing, DecoderQueueFull);
    RUN_TEST_CASE(Framing, CombinedEncodeFeedNextRelease);
}
