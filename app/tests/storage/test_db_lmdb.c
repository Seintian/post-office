#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "unity/unity_fixture.h"
#include "storage/db_lmdb.h"

static const char  *TEMPLATE     = "/tmp/dbtestXXXXXX";
static char        *dir_template = NULL;
static char        *env_path;
static db_env_t    *env;
static db_bucket_t *bucket;

TEST_GROUP(DB_LMDB);

TEST_SETUP(DB_LMDB) {
    env    = NULL;
    bucket = NULL;

    // strdup a fresh copy of the template for mkdtemp:
    dir_template = strdup(TEMPLATE);
    TEST_ASSERT_NOT_NULL(dir_template);

    env_path = mkdtemp(dir_template);
    TEST_ASSERT_NOT_NULL_MESSAGE(env_path, "mkdtemp failed");
    // keep ownership of the allocated buffer in env_path,
    // so we can free it in tear‑down
}

TEST_TEAR_DOWN(DB_LMDB) {
    if (bucket) {
        db_bucket_close(&bucket);
    }
    if (env) {
        db_env_close(&env);
    }
    // remove the on‑disk dir and free our strdup'd buffer
    if (env_path) {
        rmdir(env_path);
        free(env_path);
        env_path = NULL;
    }
}

/* helper: open a bucket in a new env */
static void open_env_and_bucket(const char *bname) {
    int rc = db_env_open(env_path, 4, 1<<20, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    rc = db_bucket_open(env, bname, &bucket);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

/* callback for iterate: collect into arrays */
typedef struct {
    int      count;
    char     keys[10][32];
    char     vals[10][32];
} iter_data_t;

static int iter_collect(const void *key, size_t keylen,
                        const void *val, size_t vallen,
                        void *udata)
{
    iter_data_t *d = udata;
    if (d->count >= 10) return 1;
    memcpy(d->keys[d->count], key, keylen);
    d->keys[d->count][keylen] = '\0';
    memcpy(d->vals[d->count], val, vallen);
    d->vals[d->count][vallen] = '\0';
    d->count++;
    return 0;
}

static int iter_stop_after_one(
    const void*,
    size_t,
    const void*,
    size_t,
    void *
) {
    /* return non-zero to stop immediately */
    return 42;
}

TEST(DB_LMDB, EnvOpenInvalidPath) {
    db_env_t *bad = NULL;
    int rc = db_env_open("/no/such/dir/hopefully", 1, 1<<20, &bad);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_NULL(bad);
}

TEST(DB_LMDB, EnvOpenClose) {
    int rc = db_env_open(env_path, 2, 1<<20, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(env);
    db_env_close(&env);
}


TEST(DB_LMDB, BucketOpenAndClose) {
    open_env_and_bucket("mybucket");
    TEST_ASSERT_NOT_NULL(bucket);
    db_bucket_close(&bucket);
}

TEST(DB_LMDB, PutGetDelete) {
    open_env_and_bucket("b1");

    /* put */
    const char *k = "hello";
    const char *v = "world";
    int rc = db_put(bucket, k, strlen(k) + 1, v, strlen(v) + 1);
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* get */
    void *out = NULL;
    size_t outlen = 0;
    rc = db_get(bucket, k, strlen(k) + 1, &out, &outlen);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT(strlen(v) + 1, outlen);
    TEST_ASSERT_EQUAL_STRING(v, (char*)out);
    free(out);

    /* delete */
    rc = db_delete(bucket, k, strlen(k) + 1);
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* get after delete */
    rc = db_get(bucket, k, strlen(k), &out, &outlen);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(DB_ENOTFOUND, errno);
}

TEST(DB_LMDB, PutOverwrite) {
    open_env_and_bucket("b2");
    const char *k = "key";
    const char *v1 = "one";
    const char *v2 = "two";

    TEST_ASSERT_EQUAL_INT(0, db_put(bucket, k, strlen(k) + 1, v1, strlen(v1) + 1));
    TEST_ASSERT_EQUAL_INT(0, db_put(bucket, k, strlen(k) + 1, v2, strlen(v2) + 1));

    void *out; size_t len;
    TEST_ASSERT_EQUAL_INT(0, db_get(bucket, k, strlen(k) + 1, &out, &len));
    TEST_ASSERT_EQUAL_UINT(strlen(v2) + 1, len);
    TEST_ASSERT_EQUAL_STRING(v2, (char*)out);
    free(out);
}

TEST(DB_LMDB, DeleteMissing) {
    open_env_and_bucket("b3");
    int rc = db_delete(bucket, "nokey", 5);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(DB_ENOTFOUND, errno);
}

TEST(DB_LMDB, GetMissing) {
    open_env_and_bucket("b4");
    void *out; size_t len;
    int rc = db_get(bucket, "nokey", 5, &out, &len);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(DB_ENOTFOUND, errno);
}

TEST(DB_LMDB, IterateAll) {
    open_env_and_bucket("b5");
    const struct {
        const char *k;
        const char *v;
    } items[] = {
        {"apple","red"}, {"banana","yellow"}, {"cherry","dark"},
    };
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(
            0,
            db_put(
                bucket,
                items[i].k,
                strlen(items[i].k) + 1,
                items[i].v,
                strlen(items[i].v) + 1
            )
        );
    }
    iter_data_t data = { .count = 0 };
    int rc = db_iterate(bucket, iter_collect, &data);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(3, data.count);
    /* entries come back in lex order by key */
    TEST_ASSERT_EQUAL_STRING("apple",  data.keys[0]);
    TEST_ASSERT_EQUAL_STRING("banana", data.keys[1]);
    TEST_ASSERT_EQUAL_STRING("cherry", data.keys[2]);
}

TEST(DB_LMDB, IterateEarlyStop) {
    open_env_and_bucket("b6");
    TEST_ASSERT_EQUAL_INT(0, db_put(bucket, "x", 1 + 1, "1", 1 + 1));
    TEST_ASSERT_EQUAL_INT(0, db_put(bucket, "y", 1 + 1, "2", 1 + 1));
    int rc = db_iterate(bucket, iter_stop_after_one, NULL);
    TEST_ASSERT_EQUAL_INT(42, rc);
}

TEST(DB_LMDB, MultipleBucketsIsolation) {
    open_env_and_bucket("bA");
    db_bucket_t *b2;
    TEST_ASSERT_EQUAL_INT(0, db_bucket_open(env, "bB", &b2));

    TEST_ASSERT_EQUAL_INT(0, db_put(bucket, "foo", 3 + 1, "ONE", 3 + 1));
    TEST_ASSERT_EQUAL_INT(0, db_put(b2,     "foo", 3 + 1, "TWO", 3 + 1));

    void *o1; size_t l1;
    TEST_ASSERT_EQUAL_INT(0, db_get(bucket, "foo", 3 + 1, &o1, &l1));
    TEST_ASSERT_EQUAL_STRING_LEN("ONE", o1, l1);
    free(o1);

    void *o2; size_t l2;
    TEST_ASSERT_EQUAL_INT(0, db_get(b2, "foo", 3 + 1, &o2, &l2));
    TEST_ASSERT_EQUAL_STRING_LEN("TWO", o2, l2);
    free(o2);

    db_bucket_close(&b2);
}

TEST_GROUP_RUNNER(DB_LMDB) {
    RUN_TEST_CASE(DB_LMDB, EnvOpenInvalidPath);
    RUN_TEST_CASE(DB_LMDB, EnvOpenClose);
    RUN_TEST_CASE(DB_LMDB, BucketOpenAndClose);
    RUN_TEST_CASE(DB_LMDB, PutGetDelete);
    RUN_TEST_CASE(DB_LMDB, PutOverwrite);
    RUN_TEST_CASE(DB_LMDB, DeleteMissing);
    RUN_TEST_CASE(DB_LMDB, GetMissing);
    RUN_TEST_CASE(DB_LMDB, IterateAll);
    RUN_TEST_CASE(DB_LMDB, IterateEarlyStop);
    RUN_TEST_CASE(DB_LMDB, MultipleBucketsIsolation);
}
