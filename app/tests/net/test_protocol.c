#include "unity/unity_fixture.h"
#include "net/protocol.h"
#include "net/framing.h"
#include "perf/zerocopy.h"
#include "utils/errors.h"

#include <string.h>
#include <errno.h>


TEST_GROUP(Protocol);

static perf_zcpool_t     *pool;
static framing_encoder_t *enc;
static framing_decoder_t *dec;

TEST_SETUP(Protocol) {
    // build a tiny zero-copy pool: 8 buffers × 256 B
    pool = perf_zcpool_create(8, 256);
    TEST_ASSERT_NOT_NULL(pool);

    // encoder uses its own pool internally
    enc = framing_encoder_new(pool, /*max_payload=*/128);
    TEST_ASSERT_NOT_NULL(enc);

    // decoder ring holds up to 8 frames
    dec = framing_decoder_new(/*depth=*/8, pool);
    TEST_ASSERT_NOT_NULL(dec);
}

TEST_TEAR_DOWN(Protocol) {
    framing_encoder_free(&enc);
    framing_decoder_free(&dec);
    perf_zcpool_destroy(&pool);
}

//-----------------------------------------------------------------------------

// round-trip a small payload
TEST(Protocol, EncodeDecodeBasic) {
    const char *msg = "hello";
    void    *frame;
    uint32_t frame_len;

    // encode into a zcpool buffer
    TEST_ASSERT_EQUAL_INT(0,
        po_proto_encode(enc,
                        /*msg_type=*/0x11, /*flags=*/0x22,
                        msg, (uint32_t) strlen(msg),
                        &frame, &frame_len));
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_UINT32(4 + PO_HEADER_SIZE + 5, frame_len);

    // hand the raw frame to the decoder
    TEST_ASSERT_EQUAL_INT(0, framing_decoder_feed(dec, frame));

    // now decode
    uint8_t  got_type;
    uint8_t  got_flags;
    void    *payload;
    uint32_t payload_len;
    int rc = po_proto_decode(dec,
                             &got_type, &got_flags,
                             &payload, &payload_len);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_UINT8(0x11, got_type);
    TEST_ASSERT_EQUAL_UINT8(0x22, got_flags);
    TEST_ASSERT_EQUAL_UINT32(5, payload_len);
    TEST_ASSERT_EQUAL_MEMORY(msg, payload, 5);

    // consumer must release the frame buffer
    perf_zcpool_release(pool, frame);

    // no more frames ready
    rc = po_proto_decode(dec,
                         &got_type, &got_flags,
                         &payload, &payload_len);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

// encode/decode an empty payload
TEST(Protocol, ZeroLengthPayload) {
    void    *frame;
    uint32_t frame_len;

    // using NULL payload pointer but zero length
    TEST_ASSERT_EQUAL_INT(0,
        po_proto_encode(enc,
                        /*msg_type=*/0x33, /*flags=*/0x44,
                        NULL, 0,
                        &frame, &frame_len));
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_UINT32(4 + PO_HEADER_SIZE + 0, frame_len);

    TEST_ASSERT_EQUAL_INT(0, framing_decoder_feed(dec, frame));

    uint8_t  mt;
    uint8_t fl;
    void    *pl;
    uint32_t plen;
    int rc = po_proto_decode(dec, &mt, &fl, &pl, &plen);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_UINT32(0, plen);

    perf_zcpool_release(pool, frame);
}

// decode on empty decoder should return 0
TEST(Protocol, DecodeEmpty) {
    uint8_t  mt;
    uint8_t  fl;
    void    *pl;
    uint32_t plen;
    int rc = po_proto_decode(dec, &mt, &fl, &pl, &plen);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

// version mismatch in the on-wire header → EPROTONOSUPPORT
TEST(Protocol, DecodeVersionMismatch) {
    // encode a valid frame first
    void    *frame; uint32_t fl;
    TEST_ASSERT_EQUAL_INT(0,
        po_proto_encode(enc,
                        /*msg_type=*/0xAA, /*flags=*/0xBB,
                        "X", 1,
                        &frame, &fl));

    // corrupt the version field (network‐order uint16 at offset 4)
    uint8_t *p = frame;
    p += 4;          // skip length prefix
    p[0] = 0x00;     // hi‐byte
    p[1] = 0x02;     // lo‐byte → version = 2

    TEST_ASSERT_EQUAL_INT(0, framing_decoder_feed(dec, frame));

    uint8_t mt;
    uint8_t flags;
    void *payload;
    uint32_t plen;
    errno = 0;
    int rc = po_proto_decode(dec, &mt, &flags, &payload, &plen);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(NET_EPROTONOSUPPORT, errno);

    perf_zcpool_release(pool, frame);
}

// too-short frame (not enough for header) → EPROTO
TEST(Protocol, DecodeMalformedShort) {
    // craft a 6-byte blob (4B length + 2B data)
    uint8_t buf[6] = { 0,0, 0,0, 0x01, 0x02 };
    // grab a pool buffer and copy only 6B into it
    void *frame = perf_zcpool_acquire(pool);
    memcpy(frame, buf, sizeof(buf));

    // feed the undersized frame
    TEST_ASSERT_EQUAL_INT(0, framing_decoder_feed(dec, frame));

    uint8_t mt;
    uint8_t fl;
    void *pl;
    uint32_t plen;
    errno = 0;
    int rc = po_proto_decode(dec, &mt, &fl, &pl, &plen);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(NET_EPROTO, errno);

    perf_zcpool_release(pool, frame);
}

TEST_GROUP_RUNNER(Protocol) {
    RUN_TEST_CASE(Protocol, EncodeDecodeBasic);
    RUN_TEST_CASE(Protocol, ZeroLengthPayload);
    RUN_TEST_CASE(Protocol, DecodeEmpty);
    RUN_TEST_CASE(Protocol, DecodeVersionMismatch);
    RUN_TEST_CASE(Protocol, DecodeMalformedShort);
}
