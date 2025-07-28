#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "unity/unity_fixture.h"
#include "perf/perf.h"
#include "utils/errors.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>


TEST_GROUP(Perf);

TEST_SETUP(Perf) {
    // Ensure clean state before each test
    perf_shutdown();
}

TEST_TEAR_DOWN(Perf) {
    perf_shutdown();
}

TEST(Perf, InitAndShutdown) {
    int rc = perf_init(4, 2, 1);
    TEST_ASSERT_EQUAL_INT(0, rc);
    // shutdown should not error
    perf_shutdown();
    // re-init after shutdown should succeed
    rc = perf_init(1,1,1);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

TEST(Perf, DoubleInitError) {
    int rc = perf_init(1,1,1);
    TEST_ASSERT_EQUAL_INT(0, rc);
    rc = perf_init(1,1,1);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(PERF_EALREADY, errno);
}

TEST(Perf, CounterBeforeInit) {
    perf_shutdown();
    const perf_counter_t *ctr = perf_counter_create("c");
    TEST_ASSERT_NULL(ctr);
    TEST_ASSERT_EQUAL_INT(PERF_ENOTINIT, errno);
}

TEST(Perf, CounterCreateAndValue) {
    perf_init(2,2,2);
    perf_counter_t *ctr1 = perf_counter_create("ct1");
    TEST_ASSERT_NOT_NULL(ctr1);
    // Recreate returns same pointer
    perf_counter_t *ctr2 = perf_counter_create("ct1");
    TEST_ASSERT_EQUAL_PTR(ctr1, ctr2);
    // initial value
    TEST_ASSERT_EQUAL_UINT64(0, perf_counter_value(ctr1));
}

TEST(Perf, CounterAddInc) {
    perf_init(1,1,1);
    perf_counter_t *ctr = perf_counter_create("ct");
    perf_counter_inc(ctr);
    TEST_ASSERT_EQUAL_UINT64(1, perf_counter_value(ctr));
    perf_counter_add(ctr, 5);
    TEST_ASSERT_EQUAL_UINT64(6, perf_counter_value(ctr));
}

TEST(Perf, TimerBeforeInit) {
    perf_shutdown();
    const perf_timer_t *t = perf_timer_create("t");
    TEST_ASSERT_NULL(t);
    TEST_ASSERT_EQUAL_INT(PERF_ENOTINIT, errno);
}

TEST(Perf, TimerCreateAndMeasure) {
    perf_init(1,1,1);
    perf_timer_t *t1 = perf_timer_create("tm");
    TEST_ASSERT_NOT_NULL(t1);
    perf_timer_t *t2 = perf_timer_create("tm");
    TEST_ASSERT_EQUAL_PTR(t1, t2);
    // perform quick start/stop
    int rc = perf_timer_start(t1);
    TEST_ASSERT_EQUAL_INT(0, rc);
    rc = perf_timer_stop(t1);
    TEST_ASSERT_EQUAL_INT(0, rc);
    uint64_t ns = perf_timer_ns(t1);
    TEST_ASSERT_TRUE(ns > 0);
}

TEST(Perf, HistogramBeforeInit) {
    perf_shutdown();
    uint64_t bins[] = {10, 20};
    const perf_histogram_t *h = perf_histogram_create("h", bins, 2);
    TEST_ASSERT_NULL(h);
}

TEST(Perf, HistogramCreateAndRecordBins) {
    perf_init(1, 1, 1);
    uint64_t bins[] = {5, 15, 30};
    const perf_histogram_t *h = perf_histogram_create("hg", bins, 3);
    TEST_ASSERT_NOT_NULL(h);
    // record values
    perf_histogram_record(h, 3);
    perf_histogram_record(h, 10);
    perf_histogram_record(h, 20);
    // Using perf_report to inspect counts
    
    // read internal counts by dumping report
    char buf[2048];
    FILE *tmp = tmpfile();
    TEST_ASSERT_NOT_NULL(tmp);

    int saved = dup(STDOUT_FILENO);
    fflush(stdout);
    dup2(fileno(tmp), STDOUT_FILENO);

    perf_report();

    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);

    rewind(tmp);
    size_t len = fread(buf, 1, sizeof(buf)-1, tmp);
    buf[len] = '\0';
    fclose(tmp);

    TEST_ASSERT_NOT_NULL(strstr(buf, "<= 5: 1"));  // first bin
    TEST_ASSERT_NOT_NULL(strstr(buf, "<= 15: 1")); // second bin
    TEST_ASSERT_NOT_NULL(strstr(buf, "<= 30: 1")); // third bin
}

TEST(Perf, HistogramOverflowBin) {
    perf_init(1, 1, 1);
    uint64_t bins[] = {1, 2};
    const perf_histogram_t *h = perf_histogram_create("of", bins, 2);
    TEST_ASSERT_NOT_NULL(h);
    // values >2 go to overflow (last bin)
    perf_histogram_record(h, 5);
    char buf[2048];
    FILE *tmp = tmpfile();
    TEST_ASSERT_NOT_NULL(tmp);

    int saved = dup(STDOUT_FILENO);
    fflush(stdout);
    dup2(fileno(tmp), STDOUT_FILENO);

    perf_report();

    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);

    rewind(tmp);
    size_t len = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[len] = '\0';
    fclose(tmp);

    TEST_ASSERT_NOT_NULL(strstr(buf, "<= 2: 1"));
}

TEST_GROUP_RUNNER(Perf) {
    RUN_TEST_CASE(Perf, InitAndShutdown);
    RUN_TEST_CASE(Perf, DoubleInitError);
    RUN_TEST_CASE(Perf, CounterBeforeInit);
    RUN_TEST_CASE(Perf, CounterCreateAndValue);
    RUN_TEST_CASE(Perf, CounterAddInc);
    RUN_TEST_CASE(Perf, TimerBeforeInit);
    RUN_TEST_CASE(Perf, TimerCreateAndMeasure);
    RUN_TEST_CASE(Perf, HistogramBeforeInit);
    RUN_TEST_CASE(Perf, HistogramCreateAndRecordBins);
    RUN_TEST_CASE(Perf, HistogramOverflowBin);
}
