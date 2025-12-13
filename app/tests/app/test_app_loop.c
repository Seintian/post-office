#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "net/net.h"
#include "net/poller.h"
#include "net/framing.h"
#include "metrics/metrics.h"
#include "perf/perf.h"
#include "storage/db_lmdb.h"
#include "unity/unity_fixture.h"

// Capture helper copied from perf tests
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

TEST_GROUP(APP);

static char *dir_template;
static char *env_path;
static db_env_t *env;
static db_bucket_t *bucket;
static poller_t *poller;

TEST_SETUP(APP) {
    // Initialize perf and framing for net module
    TEST_ASSERT_EQUAL_INT(0, po_perf_init(1, 1, 1));
    framing_init(0);
    net_init_zerocopy(16, 16, 4096);

    // Temp LMDB environment
    const char *TEMPLATE = "/tmp/appmainXXXXXX";
    dir_template = strdup(TEMPLATE);
    TEST_ASSERT_NOT_NULL(dir_template);
    env_path = mkdtemp(dir_template);
    TEST_ASSERT_NOT_NULL_MESSAGE(env_path, "mkdtemp failed");

    env = NULL;
    bucket = NULL;
    TEST_ASSERT_EQUAL_INT(0, db_env_open(env_path, 2, 1 << 20, &env));
    TEST_ASSERT_EQUAL_INT(0, db_bucket_open(env, "msgs", &bucket));

    poller = poller_create();
    TEST_ASSERT_NOT_NULL(poller);

    // Create a perf counter we'll bump upon processing
    TEST_ASSERT_NOT_EQUAL(-1, PO_METRIC_COUNTER_CREATE("processed"));
}

TEST_TEAR_DOWN(APP) {
    if (poller) {
        poller_destroy(poller);
        poller = NULL;
    }

    if (bucket)
        db_bucket_close(&bucket);

    if (env)
        db_env_close(&env);

    if (env_path) {
        rmdir(env_path);
        free(env_path);
        env_path = NULL;
    }

    // dir_template aliases env_path (mkdtemp returns the same buffer), do not free twice
    dir_template = NULL;

    // Drain and shutdown perf (also validates it doesn't crash)
    char buf[1024];
    CAPTURE_REPORT(buf, sizeof(buf), po_perf_shutdown);
}

TEST(APP, MAIN_LOOP_END_TO_END) {
    // Arrange networking
    int sv[2];
    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    // Monitor consumer side for readability
    TEST_ASSERT_EQUAL_INT(0, poller_add(poller, sv[1], EPOLLIN));

    // Send one message from producer
    const char payload[] = "hello";
    TEST_ASSERT_EQUAL_INT(0, net_send_message(sv[0], /*msg_type*/ 0x7Au, PO_FLAG_NONE,
                                              (const uint8_t *)payload, (uint32_t)sizeof payload));

    // Minimal event loop: wait and process one message
    struct epoll_event ev;
    int n = poller_wait(poller, &ev, 1, 1000);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(sv[1], ev.data.fd);
    TEST_ASSERT_TRUE((ev.events & EPOLLIN) != 0);

    po_header_t hdr;
    zcp_buffer_t *buf = NULL;
    TEST_ASSERT_EQUAL_INT(0, net_recv_message(sv[1], &hdr, &buf));
    // We now receive a valid buffer
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_HEX8(0x7Au, hdr.msg_type);
    net_zcp_release_rx(buf);

    // Persist something about the message into storage
    char key[16];
    snprintf(key, sizeof key, "msg_%02X", hdr.msg_type);
    char val[32];
    snprintf(val, sizeof val, "len=%u", hdr.payload_len);
    TEST_ASSERT_EQUAL_INT(0, db_put(bucket, key, strlen(key) + 1, val, strlen(val) + 1));

    // Update perf counter and allow worker to flush
    PO_METRIC_COUNTER_INC("processed");
    struct timespec ts = {0, 15 * 1000 * 1000}; // 5ms
    nanosleep(&ts, NULL);

    // Read back from storage
    void *out = NULL;
    size_t outlen = 0;
    TEST_ASSERT_EQUAL_INT(0, db_get(bucket, key, strlen(key) + 1, &out, &outlen));
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(1, outlen);
    TEST_ASSERT_NOT_NULL(strstr((const char *)out, "len="));
    free(out);

    // Ensure queued perf events processed before reporting
    TEST_ASSERT_EQUAL_INT(0, po_perf_flush());
    char rep[1024];
    CAPTURE_REPORT(rep, sizeof(rep), po_perf_report);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(rep, "processed: 1"), rep);

    // Cleanup fds
    close(sv[0]);
    close(sv[1]);
}

// Additional integration: exercise metrics facade, random, hashset/hashtable, argv, sysinfo,
// storage high-level API and logger sink attachment.
#include "hashset/hashset.h"
#include "hashtable/hashtable.h"
#include "log/logger.h"
#include "metrics/metrics.h"
#include "random/random.h"
#include "storage/storage.h"
#include "sysinfo/sysinfo.h"
#include "utils/argv.h"

// Local helpers for hash structures (file scope so they can be referenced in test).
static int str_compare(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}
static unsigned long str_hash(const void *p) {
    const unsigned char *s = (const unsigned char *)p;
    unsigned long h = 5381;
    int c;
    while ((c = *s++))
        h = ((h << 5) + h) + (unsigned long)c;
    return h;
}

TEST(APP, METRICS_AND_PUBLIC_APIS_SMOKE) {
    // Metrics require perf already initialized by group setup.
    TEST_ASSERT_EQUAL_INT(0, po_metrics_init());
    // Create & use several metric types via macros.
    PO_METRIC_COUNTER_INC("app.test.counter");
    PO_METRIC_COUNTER_ADD("app.test.counter.bytes", 128);
    static const uint64_t bins[] = {10, 100, 1000, 10000};
    PO_METRIC_HISTO_CREATE("app.test.hist", bins, sizeof(bins) / sizeof(bins[0]));
    PO_METRIC_HISTO_RECORD("app.test.hist", 42);
    PO_METRIC_TIMER_CREATE("app.test.timer");
    PO_METRIC_TIMER_START("app.test.timer");
    usleep(1000); // 1ms
    PO_METRIC_TIMER_STOP("app.test.timer");

    // Random utilities (use public po_rand_* API)
    uint32_t r1 = (uint32_t)po_rand_u32();
    uint32_t r2 = (uint32_t)po_rand_range_i64(1, 100); // inclusive range
    TEST_ASSERT_NOT_EQUAL(r1, r2);                     // probabilistic but fine for smoke

    // Hashset usage (create with compare + hash)
    po_hashset_t *hs = po_hashset_create(str_compare, str_hash);
    TEST_ASSERT_NOT_NULL(hs);
    TEST_ASSERT_EQUAL_INT(1, po_hashset_add(hs, "alpha"));
    TEST_ASSERT_TRUE(po_hashset_contains(hs, "alpha"));

    // Hashtable usage
    po_hashtable_t *ht = po_hashtable_create(str_compare, str_hash);
    TEST_ASSERT_NOT_NULL(ht);
    TEST_ASSERT_EQUAL_INT(1, po_hashtable_put(ht, "k1", "v1"));
    TEST_ASSERT_EQUAL_INT(1, po_hashtable_put(ht, "k2", "v2"));
    TEST_ASSERT_EQUAL_STRING("v1", (const char *)po_hashtable_get(ht, "k1"));

    // sysinfo
    po_sysinfo_t info;
    TEST_ASSERT_EQUAL_INT(0, po_sysinfo_collect(&info));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, info.physical_cores);

    // argv parsing (simulate program with log level + syslog enabled)
    po_args_t args;
    po_args_init(&args);
    char *argvv[] = {(char *)"test", (char *)"--loglevel", (char *)"2", (char *)"--syslog"};
    TEST_ASSERT_EQUAL_INT(0, po_args_parse(&args, 4, argvv, fileno(stdout)));
    TEST_ASSERT_TRUE(args.syslog);
    po_args_destroy(&args);

    // Logger + storage high-level API (attach sink)
    po_logger_config_t lcfg = {.level = LOG_INFO,
                               .ring_capacity = 256,
                               .consumers = 1,
                               .policy = LOGGER_OVERWRITE_OLDEST,
                               .cacheline_bytes = 64};
    TEST_ASSERT_EQUAL_INT(0, po_logger_init(&lcfg));
    char tdir[] = "/tmp/po_storageXXXXXX";
    TEST_ASSERT_NOT_NULL(mkdtemp(tdir));
    po_storage_config_t scfg = {.dir = tdir,
                                .bucket = "idx",
                                .map_size = 1 << 20,
                                .ring_capacity = 64,
                                .batch_size = 8,
                                .fsync_policy = PO_LS_FSYNC_NONE,
                                .attach_logger_sink = false};
    TEST_ASSERT_EQUAL_INT(0, po_storage_init(&scfg));
    // simple key/value via logstore public API (if available) skipped; ensure logstore instance
    // exists
    TEST_ASSERT_NOT_NULL(po_storage_logstore());
    po_storage_shutdown();
    po_logger_shutdown();

    // Capture perf report and assert metric names appeared
    char rep[4096];
    CAPTURE_REPORT(rep, sizeof(rep), po_perf_report);
    TEST_ASSERT_NOT_NULL(strstr(rep, "app.test.counter"));
    TEST_ASSERT_NOT_NULL(strstr(rep, "app.test.timer"));
    TEST_ASSERT_NOT_NULL(strstr(rep, "app.test.hist"));

    // Cleanup containers (APIs take pointer to pointer)
    po_hashset_destroy(&hs);
    po_hashtable_destroy(&ht);
    po_metrics_shutdown();
}

TEST_GROUP_RUNNER(APP) {
    RUN_TEST_CASE(APP, MAIN_LOOP_END_TO_END);
    RUN_TEST_CASE(APP, METRICS_AND_PUBLIC_APIS_SMOKE);
}
