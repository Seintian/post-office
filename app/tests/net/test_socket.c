#include "unity/unity_fixture.h"
#include "net/socket.h"
#include "utils/errors.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>


static po_conn_t *listener = NULL;
static po_conn_t *client   = NULL;
static po_conn_t *server   = NULL;
static char      *sockpath = NULL;

// Thread entry for client side connect
static void *client_thread_fn(void *arg) {
    (void)arg;
    client = sock_connect(sockpath, 0);
    return NULL;
}

TEST_GROUP(Socket);

TEST_SETUP(Socket) {
    char tmpdir[] = "/tmp/poXXXXXX";
    char *d = mkdtemp(tmpdir);
    TEST_ASSERT_NOT_NULL(d);

    // form sockpath = "unix:<dir>/sock"
    size_t n = strlen(d) + 6 + 6;  // "unix:" + "/sock\0"
    sockpath = malloc(n);
    TEST_ASSERT_NOT_NULL(sockpath);
    snprintf(sockpath, n, "unix:%s/sock", d);

    if (sock_init(16) < 0) {
        TEST_FAIL_MESSAGE("sock_init failed");
    }

    listener = sock_listen(sockpath, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(listener, "sock_listen failed");
}

TEST_TEAR_DOWN(Socket) {
    if (server)   { sock_close(&server); server = NULL; }
    if (client)   { sock_close(&client); client = NULL; }
    if (listener) { sock_close(&listener); listener = NULL; }
    if (sockpath) {
        // remove socket file + dir
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

    sock_shutdown();
}

TEST(Socket, AcceptAndClose) {
    // spawn connect thread
    pthread_t tid;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&tid, NULL, client_thread_fn, NULL));

    // wait a bit to ensure client tries to connect
    usleep(10000); // 10ms

    // accept
    TEST_ASSERT_EQUAL_INT(0, sock_accept(listener, &server));
    TEST_ASSERT_NOT_NULL_MESSAGE(server, "sock_accept failed");

    // client side should now have a connection
    pthread_join(tid, NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(client, "sock_connect failed");

    // closing them both
    sock_close(&server);
    sock_close(&client);
    TEST_ASSERT_NULL(server);
    TEST_ASSERT_NULL(client);
}

TEST(Socket, SendReceive_UnixDomain) {
    // accept connection concurrently
    pthread_t tid;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&tid, NULL, client_thread_fn, NULL));

    // wait a bit to ensure client tries to connect
    usleep(10000); // 10ms

    TEST_ASSERT_EQUAL_INT(0, sock_accept(listener, &server));
    pthread_join(tid, NULL);
    TEST_ASSERT_NOT_NULL(client);

    // client → server
    const char *msg = "hello, socket!";
    ssize_t sent = sock_send(client, msg, strlen(msg));
    TEST_ASSERT_EQUAL_INT((int)strlen(msg), sent);

    char buf[32] = {0};
    ssize_t recvd = sock_recv(server, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT((int)strlen(msg), recvd);
    TEST_ASSERT_EQUAL_STRING(msg, buf);

    // server → client
    const char *reply = "pong";
    sent = sock_send(server, reply, strlen(reply));
    TEST_ASSERT_EQUAL_INT((int)strlen(reply), sent);

    memset(buf, 0, sizeof(buf));
    recvd = sock_recv(client, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT((int)strlen(reply), recvd);
    TEST_ASSERT_EQUAL_STRING(reply, buf);
}

TEST(Socket, SendZeroLength) {
    // accept
    pthread_t tid;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&tid, NULL, client_thread_fn, NULL));

    // wait a bit to ensure client tries to connect
    usleep(10000); // 10ms

    TEST_ASSERT_EQUAL_INT(0, sock_accept(listener, &server));
    pthread_join(tid, NULL);
    TEST_ASSERT_NOT_NULL(client);

    // send zero-byte
    ssize_t sent = sock_send(client, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, sent);

    // recv zero-byte
    char buf[1];
    ssize_t recvd = sock_recv(server, buf, 1);
    TEST_ASSERT_EQUAL_INT(-1, recvd);
    TEST_ASSERT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
}

TEST(Socket, ConnectToNonexistent) {
    // path that doesn't exist
    const po_conn_t *c = sock_connect("unix:/tmp/does/not/exist.sock", 0);
    TEST_ASSERT_NULL(c);
    TEST_ASSERT_TRUE(errno == ENOENT || errno == ENOTCONN);
}

TEST_GROUP_RUNNER(Socket) {
    RUN_TEST_CASE(Socket, AcceptAndClose);
    RUN_TEST_CASE(Socket, SendReceive_UnixDomain);
    RUN_TEST_CASE(Socket, SendZeroLength);
    RUN_TEST_CASE(Socket, ConnectToNonexistent);
}
