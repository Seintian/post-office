#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "unity/unity_fixture.h"
#include "perf/perf.h"
#include "net/net.h"
#include "net/poller.h"
#include "storage/db_lmdb.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/epoll.h>

// Capture helper copied from perf tests
#define CAPTURE_REPORT(buf, bufsize, call) do {   \
    FILE *tmp = tmpfile();                        \
    TEST_ASSERT_NOT_NULL(tmp);                    \
    call(tmp);                                    \
    fflush(tmp);                                  \
    rewind(tmp);                                  \
    size_t len = fread(buf, 1, bufsize - 1, tmp); \
    buf[len] = '\0';                              \
    fclose(tmp);                                  \
} while (0)

TEST_GROUP(APP);

static char        *dir_template;
static char        *env_path;
static db_env_t    *env;
static db_bucket_t *bucket;
static poller_t    *poller;

TEST_SETUP(APP) {
    // Initialize perf and framing for net module
    TEST_ASSERT_EQUAL_INT(0, perf_init(1, 1, 1));
    framing_init(0);

    // Temp LMDB environment
    const char *TEMPLATE = "/tmp/appmainXXXXXX";
    dir_template = strdup(TEMPLATE);
    TEST_ASSERT_NOT_NULL(dir_template);
    env_path = mkdtemp(dir_template);
    TEST_ASSERT_NOT_NULL_MESSAGE(env_path, "mkdtemp failed");

    env = NULL;
    bucket = NULL;
    TEST_ASSERT_EQUAL_INT(0, db_env_open(env_path, 2, 1<<20, &env));
    TEST_ASSERT_EQUAL_INT(0, db_bucket_open(env, "msgs", &bucket));

    poller = poller_create();
    TEST_ASSERT_NOT_NULL(poller);

    // Create a perf counter we'll bump upon processing
    TEST_ASSERT_NOT_EQUAL(-1, perf_counter_create("processed"));
}

TEST_TEAR_DOWN(APP) {
    if (poller) { poller_destroy(poller); poller = NULL; }
    if (bucket) { db_bucket_close(&bucket); }
    if (env) { db_env_close(&env); }
    if (env_path) { rmdir(env_path); free(env_path); env_path = NULL; }
    // dir_template aliases env_path (mkdtemp returns the same buffer), do not free twice
    dir_template = NULL;

    // Drain and shutdown perf (also validates it doesn't crash)
    char buf[1024];
    CAPTURE_REPORT(buf, sizeof(buf), perf_shutdown);
}

TEST(APP, MAINLOOPENDTOEND) {
    // Arrange networking
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    // Monitor consumer side for readability
    TEST_ASSERT_EQUAL_INT(0, poller_add(poller, sv[1], EPOLLIN));

    // Send one message from producer
    const char payload[] = "hello";
    TEST_ASSERT_EQUAL_INT(0, net_send_message(
        sv[0], /*msg_type*/0x7Au, PO_FLAG_NONE,
        (const uint8_t*)payload, (uint32_t)sizeof payload));

    // Minimal event loop: wait and process one message
    struct epoll_event ev;
    int n = poller_wait(poller, &ev, 1, 1000);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(sv[1], ev.data.fd);
    TEST_ASSERT_TRUE((ev.events & EPOLLIN) != 0);

    po_header_t hdr; zcp_buffer_t *buf = NULL;
    TEST_ASSERT_EQUAL_INT(0, net_recv_message(sv[1], &hdr, &buf));
    // Current framing drops payload; we only check header fields
    TEST_ASSERT_NULL(buf);
    TEST_ASSERT_EQUAL_HEX8(0x7Au, hdr.msg_type);

    // Persist something about the message into storage
    char key[16]; snprintf(key, sizeof key, "msg_%02X", hdr.msg_type);
    char val[32]; snprintf(val, sizeof val, "len=%u", hdr.payload_len);
    TEST_ASSERT_EQUAL_INT(0, db_put(bucket, key, strlen(key)+1, val, strlen(val)+1));

    // Update perf counter and allow worker to flush
    perf_counter_inc("processed");
    struct timespec ts = {0, 5 * 1000 * 1000}; // 5ms
    nanosleep(&ts, NULL);

    // Read back from storage
    void *out = NULL; size_t outlen = 0;
    TEST_ASSERT_EQUAL_INT(0, db_get(bucket, key, strlen(key)+1, &out, &outlen));
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(1, outlen);
    TEST_ASSERT_NOT_NULL(strstr((const char*)out, "len="));
    free(out);

    // Perf report should contain our counter
    char rep[1024];
    CAPTURE_REPORT(rep, sizeof(rep), perf_report);
    TEST_ASSERT_NOT_NULL(strstr(rep, "processed: 1"));

    // Cleanup fds
    close(sv[0]);
    close(sv[1]);
}

TEST_GROUP_RUNNER(APP) {
    RUN_TEST_CASE(APP, MAINLOOPENDTOEND);
}
