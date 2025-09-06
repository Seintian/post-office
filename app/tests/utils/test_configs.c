#include <stdlib.h>
#include <string.h>

#include "unity/unity_fixture.h"
#include "utils/configs.h"

static const char good_ini[] = "[global]\n"
                               "key1 = hello\n"
                               "number = 123\n"
                               "flag = 1\n";

static const char bad_ini[] = "[global]\n"
                              "key1 hello\n"; // missing '='

TEST_GROUP(CONFIGS);

TEST_SETUP(CONFIGS) {
    // nothing
}

TEST_TEAR_DOWN(CONFIGS) {
    // nothing
}

// Helper to write temp file
static char *mktempfile(const char *content) {
    char template[] = "/tmp/po_test_XXXXXX";
    int fd = mkstemp(template);
    TEST_ASSERT_TRUE(fd >= 0);
    FILE *f = fdopen(fd, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs(content, f);
    fclose(f);
    return strdup(template);
}

TEST(CONFIGS, LOAD_SUCCESS) {
    char *path = mktempfile(good_ini);
    po_config_t *cfg;
    int rc = po_config_load(path, &cfg);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

TEST(CONFIGS, LOAD_FAILURE_MALFORMED) {
    char *path = mktempfile(bad_ini);
    po_config_t *cfg;
    int rc = po_config_load_strict(path, &cfg);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_NULL(cfg);
}

TEST(CONFIGS, GET_STRING_SUCCESS) {
    char *path = mktempfile(good_ini);
    po_config_t *cfg;
    po_config_load(path, &cfg);
    const char *val;
    int rc = po_config_get_str(cfg, "global", "key1", &val);
    TEST_ASSERT_EQUAL_STRING("hello", val);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

TEST(CONFIGS, GET_STRING_MISSING) {
    char *path = mktempfile(good_ini);
    po_config_t *cfg;
    po_config_load(path, &cfg);
    const char *val;
    int rc = po_config_get_str(cfg, "global", "nokey", &val);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_NOT_EQUAL(0, rc);
}

TEST(CONFIGS, GET_INT_SUCCESS) {
    char *path = mktempfile(good_ini);
    po_config_t *cfg;
    po_config_load(path, &cfg);
    long num;
    int rc = po_config_get_long(cfg, "global", "number", &num);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(123, num);
}

TEST(CONFIGS, GET_INT_INVALID) {
    char content[] = "[global]\nnumber=notanumber\n";
    char *path = mktempfile(content);
    po_config_t *cfg;
    po_config_load(path, &cfg);
    long num;
    int rc = po_config_get_long(cfg, "global", "number", &num);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

TEST(CONFIGS, GET_BOOL_SUCCESS) {
    char *path = mktempfile(good_ini);
    po_config_t *cfg;
    po_config_load(path, &cfg);
    bool flag;
    int rc = po_config_get_bool(cfg, "global", "flag", &flag);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_TRUE(flag);
}

TEST(CONFIGS, GET_BOOL_INVALID) {
    char content[] = "[global]\nflag=maybe\n";
    char *path = mktempfile(content);
    po_config_t *cfg;
    po_config_load(path, &cfg);
    bool flag;
    int rc = po_config_get_bool(cfg, "global", "flag", &flag);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_EQUAL_INT(-1, rc);
}

TEST_GROUP_RUNNER(CONFIGS) {
    RUN_TEST_CASE(CONFIGS, LOAD_SUCCESS);
    RUN_TEST_CASE(CONFIGS, LOAD_FAILURE_MALFORMED);
    RUN_TEST_CASE(CONFIGS, GET_STRING_SUCCESS);
    RUN_TEST_CASE(CONFIGS, GET_STRING_MISSING);
    RUN_TEST_CASE(CONFIGS, GET_INT_SUCCESS);
    RUN_TEST_CASE(CONFIGS, GET_INT_INVALID);
    RUN_TEST_CASE(CONFIGS, GET_BOOL_SUCCESS);
    RUN_TEST_CASE(CONFIGS, GET_BOOL_INVALID);
}
