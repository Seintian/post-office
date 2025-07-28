#include "unity/unity_fixture.h"
#include "utils/configs.h"
#include <string.h>
#include <stdlib.h>


static const char good_ini[] =
    "[global]\n"
    "key1 = hello\n"
    "number = 123\n"
    "flag = 1\n";

static const char bad_ini[] =
    "[global]\n"
    "key1 hello\n";  // missing '='


TEST_GROUP(Configs);

TEST_SETUP(Configs) {
    // nothing
}

TEST_TEAR_DOWN(Configs) {
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

TEST(Configs, LoadSuccess) {
    char *path = mktempfile(good_ini);
    po_config_t *cfg;
    int rc = po_config_load(path, &cfg);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

TEST(Configs, LoadFailureMalformed) {
    char *path = mktempfile(bad_ini);
    po_config_t *cfg;
    int rc = po_config_load_strict(path, &cfg);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_NULL(cfg);
}

TEST(Configs, GetStringSuccess) {
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

TEST(Configs, GetStringMissing) {
    char *path = mktempfile(good_ini);
    po_config_t *cfg;
    po_config_load(path, &cfg);
    const char *val;
    int rc = po_config_get_str(cfg, "global", "nokey", &val);
    po_config_free(&cfg);
    free(path);
    TEST_ASSERT_NOT_EQUAL(0, rc);
}

TEST(Configs, GetIntSuccess) {
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

TEST(Configs, GetIntInvalid) {
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

TEST(Configs, GetBoolSuccess) {
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

TEST(Configs, GetBoolInvalid) {
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

TEST_GROUP_RUNNER(Configs) {
    RUN_TEST_CASE(Configs, LoadSuccess);
    RUN_TEST_CASE(Configs, LoadFailureMalformed);
    RUN_TEST_CASE(Configs, GetStringSuccess);
    RUN_TEST_CASE(Configs, GetStringMissing);
    RUN_TEST_CASE(Configs, GetIntSuccess);
    RUN_TEST_CASE(Configs, GetIntInvalid);
    RUN_TEST_CASE(Configs, GetBoolSuccess);
    RUN_TEST_CASE(Configs, GetBoolInvalid);
}
