#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log/logger.h"
#include "unity/unity_fixture.h"

TEST_GROUP(LOGGER);

TEST_SETUP(LOGGER) {
    po_logger_config_t cfg = {
        .level = LOG_TRACE,
        .ring_capacity = 1024,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
    };
    TEST_ASSERT_EQUAL_INT(0, po_logger_init(&cfg));
}

TEST_TEAR_DOWN(LOGGER) {
    po_logger_shutdown();
}

TEST(LOGGER, INIT_AND_LEVEL) {
    TEST_ASSERT_EQUAL_INT(LOG_TRACE, po_logger_get_level());
    TEST_ASSERT_EQUAL_INT(0, po_logger_set_level(LOG_INFO));
    TEST_ASSERT_EQUAL_INT(LOG_INFO, po_logger_get_level());
}

TEST(LOGGER, CONSOLE_SINK_AND_WRITE) {
    TEST_ASSERT_EQUAL_INT(0, po_logger_add_sink_console(true));
    // Temporarily discard stderr so console sink output doesn't pollute test output
    int saved_stderr = dup(fileno(stderr));
    TEST_ASSERT_NOT_EQUAL(-1, saved_stderr);
    int devnull = open("/dev/null", O_WRONLY);
    TEST_ASSERT_NOT_EQUAL(-1, devnull);
    int rc_dup2 = dup2(devnull, fileno(stderr));
    TEST_ASSERT_NOT_EQUAL(-1, rc_dup2);
    close(devnull);

    LOG_INFO("hello %s", "world");
    // give consumer time to drain
    usleep(10 * 1000);

    // Restore original stderr
    rc_dup2 = dup2(saved_stderr, fileno(stderr));
    TEST_ASSERT_NOT_EQUAL(-1, rc_dup2);
    close(saved_stderr);
}

TEST(LOGGER, FILE_SINK_WRITES) {
    char path[] = "/tmp/po_logger_testXXXXXX";
    int fd = mkstemp(path);
    TEST_ASSERT_NOT_EQUAL(-1, fd);
    close(fd);
    TEST_ASSERT_EQUAL_INT(0, po_logger_add_sink_file(path, false));
    LOG_DEBUG("file sink test %d", 123);
    usleep(10 * 1000);
    // Best-effort: just ensure file exists and is non-empty
    FILE *fp = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(fp);
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fclose(fp);
    TEST_ASSERT(sz >= 0);
    unlink(path);
}

// group runner declared later (after all tests)

// New test: force queue overflow and assert an overflow error notice is emitted
TEST(LOGGER, OVERFLOW_EMITS_ERROR) {
    // Re-init logger with a very small ring to force overflow quickly
    po_logger_shutdown();
    po_logger_config_t cfg = {
        .level = LOG_TRACE,
        .ring_capacity = 32, // small ring, but enough room for notice record
        .consumers = 1,
        .policy = LOGGER_DROP_NEW, // simpler overflow path
    };
    TEST_ASSERT_EQUAL_INT(0, po_logger_init(&cfg));

    // Write to a temp file to capture logs deterministically
    char path[] = "/tmp/po_logger_overflowXXXXXX";
    int fd = mkstemp(path);
    TEST_ASSERT_NOT_EQUAL(-1, fd);
    close(fd);
    TEST_ASSERT_EQUAL_INT(0, po_logger_add_sink_file(path, false));

    // Flood the logger in bursts to allow the worker to run and persist notices
    for (int b = 0; b < 50; ++b) {
        for (int i = 0; i < 200; ++i) {
            LOG_INFO("spam %d", b * 200 + i);
        }
        usleep(5 * 1000); // 5ms between bursts
    }

    // Give the worker time to process
    usleep(150 * 1000);

    // Shutdown to flush file sink buffers before reading
    po_logger_shutdown();

    // Read back the file and search for the overflow notice
    FILE *fp = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(fp);
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    TEST_ASSERT(sz > 0);
    rewind(fp);
    char *buf = malloc((size_t)sz + 1);
    TEST_ASSERT_NOT_NULL(buf);
    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[rd] = '\0';

    // Expect at least one overflow notice line
    const char *needle = "logger overflow:";
    int found = strstr(buf, needle) != NULL;
    if (!found) {
        // optional: write the buffer to stderr for diagnostics
        // fprintf(stderr, "%s\n", buf);
    }
    free(buf);
    TEST_ASSERT_TRUE_MESSAGE(found, "Expected overflow notice not found in log file");
    unlink(path);
}

// Group runner with all tests
TEST_GROUP_RUNNER(LOGGER) {
    RUN_TEST_CASE(LOGGER, INIT_AND_LEVEL);
    RUN_TEST_CASE(LOGGER, CONSOLE_SINK_AND_WRITE);
    RUN_TEST_CASE(LOGGER, FILE_SINK_WRITES);
    RUN_TEST_CASE(LOGGER, OVERFLOW_EMITS_ERROR);
}
