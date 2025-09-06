#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "perf/perf.h"
#include "unity/unity_fixture.h"
#include "utils/errors.h"

#define CAPTURE_REPORT(buf, bufsize, call)                                                         \
    do {                                                                                           \
        FILE *tmp = tmpfile();                                                                     \
        TEST_ASSERT_NOT_NULL(tmp);                                                                 \
        call(tmp);                                                                                 \
        fflush(tmp);                                                                               \
        rewind(tmp);                                                                               \
        size_t len = fread(buf, 1, bufsize - 1, tmp);                                              \
        buf[len] = '\0';                                                                           \
        fclose(tmp);                                                                               \
    } while (0)

TEST_GROUP(PERF);

TEST_SETUP(PERF) {
    char buf[2048];
    CAPTURE_REPORT(buf, sizeof(buf), perf_shutdown);
}

TEST_TEAR_DOWN(PERF) {
    char buf[2048];
    CAPTURE_REPORT(buf, sizeof(buf), perf_shutdown);
}

TEST(PERF, INIT_AND_SHUTDOWN) {
    TEST_ASSERT_EQUAL_INT(0, perf_init(4, 2, 1));
    char buf[2048];
    CAPTURE_REPORT(buf, sizeof(buf), perf_shutdown);
    TEST_ASSERT_EQUAL_INT(0, perf_init(1, 1, 1)); // reinit works
}

TEST(PERF, DOUBLE_INIT_ERROR) {
    TEST_ASSERT_EQUAL_INT(0, perf_init(1, 1, 1));
    TEST_ASSERT_EQUAL_INT(-1, perf_init(1, 1, 1));
    TEST_ASSERT_EQUAL_INT(PERF_EALREADY, errno);
}

TEST(PERF, COUNTER_BEFORE_INIT) {
    perf_shutdown(stdout);
    TEST_ASSERT_EQUAL_INT(-1, perf_counter_create("c"));
    TEST_ASSERT_EQUAL_INT(PERF_ENOTINIT, errno);
}

TEST(PERF, COUNTER_CREATE_AND_INCREMENT) {
    TEST_ASSERT_EQUAL_INT(0, perf_init(2, 2, 2));
    TEST_ASSERT_NOT_EQUAL_INT(-1, perf_counter_create("ct"));

    // queue updates
    perf_counter_inc("ct");
    perf_counter_add("ct", 3);

    // wait for worker to flush events
    struct timespec ts = {0, 10000 * 1000}; // 10000 microseconds = 10 milliseconds
    nanosleep(&ts, NULL);

    char buf[1024];
    CAPTURE_REPORT(buf, sizeof(buf), perf_report);
    TEST_ASSERT_NOT_NULL(strstr(buf, "ct: 4")); // 1+3
}

TEST(PERF, TIMER_BEFORE_INIT) {
    perf_shutdown(stdout);
    TEST_ASSERT_EQUAL_INT(-1, perf_timer_create("t"));
    TEST_ASSERT_EQUAL_INT(PERF_ENOTINIT, errno);
}

TEST(PERF, TIMER_CREATE_AND_MEASURE) {
    perf_init(1, 1, 1);
    TEST_ASSERT_EQUAL_INT(0, perf_timer_create("tm"));

    TEST_ASSERT_EQUAL_INT(0, perf_timer_start("tm"));
    struct timespec ts = {0, 5000 * 1000}; // 5000 microseconds = 5 milliseconds
    nanosleep(&ts, NULL);
    TEST_ASSERT_EQUAL_INT(0, perf_timer_stop("tm"));

    struct timespec ts2 = {0, 10000 * 1000}; // 10000 microseconds = 10 milliseconds
    nanosleep(&ts2, NULL);

    char buf[1024];
    CAPTURE_REPORT(buf, sizeof(buf), perf_report);
    TEST_ASSERT_NOT_NULL(strstr(buf, "tm:")); // value > 0 printed
}

TEST(PERF, HISTOGRAM_BEFORE_INIT) {
    perf_shutdown(stdout);
    uint64_t bins[] = {10, 20};
    TEST_ASSERT_EQUAL_INT(-1, perf_histogram_create("h", bins, 2));
}

TEST(PERF, HISTOGRAM_CREATE_AND_RECORD_BINS) {
    perf_init(1, 1, 1);
    uint64_t bins[] = {5, 15, 30};
    TEST_ASSERT_EQUAL_INT(0, perf_histogram_create("hg", bins, 3));

    TEST_ASSERT_EQUAL_INT(0, perf_histogram_record("hg", 3));
    TEST_ASSERT_EQUAL_INT(0, perf_histogram_record("hg", 10));
    TEST_ASSERT_EQUAL_INT(0, perf_histogram_record("hg", 20));

    struct timespec ts = {0, 10000 * 1000}; // 10000 microseconds = 10 milliseconds
    nanosleep(&ts, NULL);

    char buf[2048];
    CAPTURE_REPORT(buf, sizeof(buf), perf_report);

    TEST_ASSERT_NOT_NULL(strstr(buf, "<= 5: 1"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<= 15: 1"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<= 30: 1"));
}

TEST(PERF, HISTOGRAM_OVERFLOW_BIN) {
    perf_init(1, 1, 1);
    uint64_t bins[] = {1, 2};
    TEST_ASSERT_EQUAL_INT(0, perf_histogram_create("of", bins, 2));

    TEST_ASSERT_EQUAL_INT(0, perf_histogram_record("of", 5)); // goes to overflow bin

    struct timespec ts = {0, 10000 * 1000}; // 10000 microseconds = 10 milliseconds
    nanosleep(&ts, NULL);

    char buf[2048];
    CAPTURE_REPORT(buf, sizeof(buf), perf_report);

    TEST_ASSERT_NOT_NULL(strstr(buf, "<= 2: 1"));
}

TEST_GROUP_RUNNER(PERF) {
    RUN_TEST_CASE(PERF, INIT_AND_SHUTDOWN);
    RUN_TEST_CASE(PERF, DOUBLE_INIT_ERROR);
    RUN_TEST_CASE(PERF, COUNTER_BEFORE_INIT);
    RUN_TEST_CASE(PERF, COUNTER_CREATE_AND_INCREMENT);
    RUN_TEST_CASE(PERF, TIMER_BEFORE_INIT);
    RUN_TEST_CASE(PERF, TIMER_CREATE_AND_MEASURE);
    RUN_TEST_CASE(PERF, HISTOGRAM_BEFORE_INIT);
    RUN_TEST_CASE(PERF, HISTOGRAM_CREATE_AND_RECORD_BINS);
    RUN_TEST_CASE(PERF, HISTOGRAM_OVERFLOW_BIN);
}
