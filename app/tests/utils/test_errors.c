#include "unity/unity_fixture.h"
#include "utils/errors.h"
#include <string.h>
#include <errno.h>

TEST_GROUP(ERRORS);

TEST_SETUP(ERRORS) {}
TEST_TEAR_DOWN(ERRORS) {}

TEST(ERRORS, INIH_Errors) {
    TEST_ASSERT_EQUAL_STRING("Success", po_strerror(INIH_EOK));
    TEST_ASSERT_EQUAL_STRING("No section found", po_strerror(INIH_ENOSECTION));
    TEST_ASSERT_EQUAL_STRING("Unknown section", po_strerror(INIH_EUNKSECTION));
}

TEST(ERRORS, DB_Errors) {
    TEST_ASSERT_EQUAL_STRING("Success", po_strerror(DB_EOK));
    TEST_ASSERT_EQUAL_STRING("Key already exists", po_strerror(DB_EKEYEXIST));
    TEST_ASSERT_EQUAL_STRING("Database corrupted", po_strerror(DB_ECORRUPTED));
}

TEST(ERRORS, PERF_Errors) {
    TEST_ASSERT_EQUAL_STRING("Success", po_strerror(PERF_EOK));
    TEST_ASSERT_EQUAL_STRING("Counter not found", po_strerror(PERF_ENOCOUNTER));
    TEST_ASSERT_EQUAL_STRING("Already initialized", po_strerror(PERF_EALREADY));
    TEST_ASSERT_EQUAL_STRING("Performance subsystem not initialized", po_strerror(PERF_ENOTINIT));
}

TEST(ERRORS, NET_Errors) {
    TEST_ASSERT_EQUAL_STRING("Success", po_strerror(NET_EOK));
    TEST_ASSERT_EQUAL_STRING("Socket error", po_strerror(NET_ESOCK));
    TEST_ASSERT_EQUAL_STRING("Protocol version mismatch", po_strerror(NET_EVERSION));
    TEST_ASSERT_EQUAL_STRING("I/O error", po_strerror(NET_EIO));
}

TEST(ERRORS, ZCP_Errors) {
    TEST_ASSERT_EQUAL_STRING("No error", po_strerror(ZCP_EOK));
    TEST_ASSERT_EQUAL_STRING("Out of memory", po_strerror(ZCP_ENOMEM));
    TEST_ASSERT_EQUAL_STRING("Memory mapping failed", po_strerror(ZCP_EMMAP));
}

TEST(ERRORS, System_Errors) {
    TEST_ASSERT_EQUAL_STRING(strerror(EINVAL), po_strerror(EINVAL));
    TEST_ASSERT_EQUAL_STRING(strerror(ENOENT), po_strerror(ENOENT));
}

TEST_GROUP_RUNNER(ERRORS) {
    RUN_TEST_CASE(ERRORS, INIH_Errors);
    RUN_TEST_CASE(ERRORS, DB_Errors);
    RUN_TEST_CASE(ERRORS, PERF_Errors);
    RUN_TEST_CASE(ERRORS, NET_Errors);
    RUN_TEST_CASE(ERRORS, ZCP_Errors);
    RUN_TEST_CASE(ERRORS, System_Errors);
}
