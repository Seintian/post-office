#include "unity/unity_fixture.h"
#include "net/net.h"
#include "perf/zerocopy.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static po_conn_t *listener = NULL;
static po_conn_t *client   = NULL;
static po_conn_t *server   = NULL;
static char      *sockpath  = NULL;

static void *client_thread_fn(void *arg) {
    (void)arg;
    client = po_connect(sockpath, 0);
    return NULL;
}

TEST_GROUP(Net);

TEST_SETUP(Net) {
    char sockdir[] = "/tmp/poXXXXXX";
    char *d = mkdtemp(sockdir);
    TEST_ASSERT_NOT_NULL_MESSAGE(d, "mkdtemp failed");

    sockpath = malloc(strlen(d) + 11);
    TEST_ASSERT_NOT_NULL(sockpath);
    sprintf(sockpath, "unix:%s/sock", d);

    int rc = po_init(16, 8, 256, 8);  // max_events, buf_count, buf_size, ring_depth
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "po_init failed");

    listener = po_listen(sockpath, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(listener, "po_listen failed");
}

TEST_TEAR_DOWN(Net) {
    if (server)   { po_close(&server); server = NULL; }
    if (client)   { po_close(&client); client = NULL; }
    if (listener) { po_close(&listener); listener = NULL; }

    if (sockpath) {
        unlink(sockpath);
        char *dir = strdup(sockpath);
        char *slash = strrchr(dir, '/');
        if (slash) {
            *slash = '\0';
            rmdir(dir);
        }
        free(dir);
        free(sockpath);
        sockpath = NULL;
    }

    po_shutdown();
}

TEST(Net, AcceptAndClose) {
    pthread_t tid;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&tid, NULL, client_thread_fn, NULL));
    usleep(10000); // allow client to attempt connect

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, po_accept(listener, &server), "po_accept failed");
    TEST_ASSERT_NOT_NULL(server);

    pthread_join(tid, NULL);
    TEST_ASSERT_NOT_NULL(client);

    po_close(&server);
    po_close(&client);

    TEST_ASSERT_NULL(server);
    TEST_ASSERT_NULL(client);
}

TEST(Net, SendAndReceiveMsg) {
    pthread_t tid;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&tid, NULL, client_thread_fn, NULL));
    usleep(10000);

    TEST_ASSERT_EQUAL_INT(0, po_accept(listener, &server));
    pthread_join(tid, NULL);
    TEST_ASSERT_NOT_NULL(client);

    const char *msg = "hello framed";
    TEST_ASSERT_EQUAL_INT(0, po_send_msg(client, 0x42, 0x99, msg, (uint32_t)strlen(msg)));

    uint8_t msg_type;
    uint8_t flags;
    void *payload;
    uint32_t plen;

    for (int i = 0; i < 50; i++) {
        int rc = po_recv_msg(server, &msg_type, &flags, &payload, &plen);
        if (rc == 1) {
            TEST_ASSERT_EQUAL_UINT8(0x42, msg_type);
            TEST_ASSERT_EQUAL_UINT8(0x99, flags);
            TEST_ASSERT_EQUAL_UINT32((uint32_t)strlen(msg), plen);
            TEST_ASSERT_EQUAL_MEMORY(msg, payload, plen);

            // release buffer back to pool
            perf_zcpool_release(server->decoder->pool,
                                (char*)payload - sizeof(po_header_t) - 4);
            return;
        }
        usleep(1000);
    }
    TEST_FAIL_MESSAGE("Message not received in time");
}

TEST(Net, SendZeroLengthMsg) {
    pthread_t tid;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&tid, NULL, client_thread_fn, NULL));
    usleep(10000);

    TEST_ASSERT_EQUAL_INT(0, po_accept(listener, &server));
    pthread_join(tid, NULL);
    TEST_ASSERT_NOT_NULL(client);

    TEST_ASSERT_EQUAL_INT(0, po_send_msg(client, 0x10, 0x01, NULL, 0));

    uint8_t msg_type;
    uint8_t flags;
    void *payload;
    uint32_t plen;

    for (int i = 0; i < 50; i++) {
        int rc = po_recv_msg(server, &msg_type, &flags, &payload, &plen);
        if (rc == 1) {
            TEST_ASSERT_EQUAL_UINT8(0x10, msg_type);
            TEST_ASSERT_EQUAL_UINT8(0x01, flags);
            TEST_ASSERT_EQUAL_UINT32(0, plen);
            perf_zcpool_release(server->decoder->pool,
                                (char*)payload - sizeof(po_header_t) - 4);
            return;
        }
        usleep(1000);
    }
    TEST_FAIL_MESSAGE("Zero-length message not received");
}

TEST(Net, ConnectToNonexistent) {
    const po_conn_t *c = po_connect("unix:/tmp/doesnotexist.sock", 0);
    TEST_ASSERT_NULL(c);
    TEST_ASSERT_TRUE(errno == ENOENT || errno == ECONNREFUSED);
}

TEST_GROUP_RUNNER(Net) {
    RUN_TEST_CASE(Net, AcceptAndClose);
    RUN_TEST_CASE(Net, SendAndReceiveMsg);
    RUN_TEST_CASE(Net, SendZeroLengthMsg);
    RUN_TEST_CASE(Net, ConnectToNonexistent);
}
