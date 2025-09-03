#include "unity/unity_fixture.h"
#include "utils/files.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

TEST_GROUP(FILES);

TEST_SETUP(FILES) {
    // nothing
}

TEST_TEAR_DOWN(FILES) {
    // nothing
}

static char *mktempfile_with_content(const char *content, size_t len) {
    char template_path[] = "/tmp/po_files_test_XXXXXX";
    int fd = mkstemp(template_path);
    TEST_ASSERT_TRUE(fd >= 0);
    FILE *f = fdopen(fd, "wb");
    TEST_ASSERT_NOT_NULL(f);
    if (content && len > 0) {
        size_t wr = fwrite(content, 1u, len, f);
        TEST_ASSERT_EQUAL_size_t(len, wr);
    }
    int rc = fclose(f);
    TEST_ASSERT_EQUAL_INT(0, rc);
    return strdup(template_path);
}

static char *mktempdir(void) {
    char template_path[] = "/tmp/po_files_dir_XXXXXX";
    char *dir = mkdtemp(template_path);
    TEST_ASSERT_NOT_NULL(dir);
    return strdup(dir);
}

TEST(FILES, EXISTS_AND_TYPES) {
    // Non-existent
    TEST_ASSERT_FALSE(fs_exists("/tmp/po_files_no_such_file"));
    TEST_ASSERT_FALSE(fs_is_regular_file("/tmp/po_files_no_such_file"));
    TEST_ASSERT_FALSE(fs_is_directory("/tmp/po_files_no_such_file"));
    TEST_ASSERT_FALSE(fs_is_socket("/tmp/po_files_no_such_file"));

    // NULL path
    TEST_ASSERT_FALSE(fs_exists(NULL));
    TEST_ASSERT_FALSE(fs_is_regular_file(NULL));
    TEST_ASSERT_FALSE(fs_is_directory(NULL));
    TEST_ASSERT_FALSE(fs_is_socket(NULL));

    // Regular file
    const char msg[] = "hello world";
    char *file = mktempfile_with_content(msg, sizeof(msg) - 1u);
    TEST_ASSERT_TRUE(fs_exists(file));
    TEST_ASSERT_TRUE(fs_is_regular_file(file));
    TEST_ASSERT_FALSE(fs_is_directory(file));
    TEST_ASSERT_FALSE(fs_is_socket(file));

    // Directory
    char *dir = mktempdir();
    TEST_ASSERT_TRUE(fs_exists(dir));
    TEST_ASSERT_TRUE(fs_is_directory(dir));
    TEST_ASSERT_FALSE(fs_is_regular_file(dir));
    TEST_ASSERT_FALSE(fs_is_socket(dir));

    // UNIX domain socket
    // Create a socket file at a temporary path
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    // construct socket path under temp dir
    char *sock_path = fs_path_join(dir, "sock");
    TEST_ASSERT_NOT_NULL(sock_path);
    size_t maxlen = sizeof(addr.sun_path) - 1u;
    TEST_ASSERT_TRUE(strlen(sock_path) < maxlen);
    strncpy(addr.sun_path, sock_path, maxlen);
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    TEST_ASSERT_TRUE(sfd >= 0);
    // Ensure no pre-existing file
    unlink(sock_path);
    int brc = bind(sfd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr));
    TEST_ASSERT_EQUAL_INT(0, brc);
    // Now a socket node should exist on filesystem
    TEST_ASSERT_TRUE(fs_exists(sock_path));
    TEST_ASSERT_TRUE(fs_is_socket(sock_path));
    TEST_ASSERT_FALSE(fs_is_directory(sock_path));
    TEST_ASSERT_FALSE(fs_is_regular_file(sock_path));
    close(sfd);
    unlink(sock_path);

    // Cleanup
    unlink(file);
    rmdir(dir);
    free(file);
    free(dir);
    free(sock_path);
}

TEST(FILES, READ_AND_WRITE_FILE) {
    const char payload[] = "abc\n123\n";
    char *path = mktempfile_with_content(NULL, 0u);

    // write
    bool wrc = fs_write_buffer_to_file(path, payload, sizeof(payload) - 1u);
    TEST_ASSERT_TRUE(wrc);

    // read
    size_t sz = 0;
    char *buf = fs_read_file_to_buffer(path, &sz);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_size_t(sizeof(payload) - 1u, sz);
    TEST_ASSERT_EQUAL_MEMORY(payload, buf, sz);
    free(buf);

    // invalid inputs
    errno = 0;
    TEST_ASSERT_FALSE(fs_write_buffer_to_file(NULL, payload, sizeof(payload) - 1u));
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);

    size_t out_sz = 111u;
    char *nope = fs_read_file_to_buffer("/tmp/po_files_surely_missing", &out_sz);
    TEST_ASSERT_NULL(nope);
    TEST_ASSERT_EQUAL_size_t(0u, out_sz);

    unlink(path);
    free(path);
}

TEST(FILES, MKDIR_P_AND_JOIN) {
    char *root = mktempdir();
    // Build nested path: root/a/b/c
    char *p1 = fs_path_join(root, "a");
    char *p2 = fs_path_join(p1, "b");
    char *p3 = fs_path_join(p2, "c");
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_NULL(p3);

    // Ensure it doesn't exist yet
    TEST_ASSERT_FALSE(fs_is_directory(p3));

    bool ok = fs_create_directory_recursive(p3, 0755);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(fs_is_directory(p3));

    // Idempotent
    bool ok2 = fs_create_directory_recursive(p3, 0755);
    TEST_ASSERT_TRUE(ok2);

    // Join behavior with trailing slash
    char *with_slash = fs_path_join("/tmp/", "leaf");
    TEST_ASSERT_EQUAL_STRING("/tmp/leaf", with_slash);
    char *no_slash = fs_path_join("/tmp", "leaf");
    TEST_ASSERT_EQUAL_STRING("/tmp/leaf", no_slash);

    // NULL handling
    char *only_leaf = fs_path_join(NULL, "leaf");
    TEST_ASSERT_EQUAL_STRING("leaf", only_leaf);
    char *only_base = fs_path_join("/base", NULL);
    TEST_ASSERT_EQUAL_STRING("/base", only_base);

    // Cleanup
    rmdir(p3);
    rmdir(p2);
    rmdir(p1);
    rmdir(root);
    free(root);
    free(p1);
    free(p2);
    free(p3);
    free(with_slash);
    free(no_slash);
    free(only_leaf);
    free(only_base);
}

TEST_GROUP_RUNNER(FILES) {
    RUN_TEST_CASE(FILES, EXISTS_AND_TYPES);
    RUN_TEST_CASE(FILES, READ_AND_WRITE_FILE);
    RUN_TEST_CASE(FILES, MKDIR_P_AND_JOIN);
}
