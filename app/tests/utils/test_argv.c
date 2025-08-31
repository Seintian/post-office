#include "unity/unity_fixture.h"
#include "utils/argv.h"
#include <getopt.h>  // for optind
#include <unistd.h>  // for STDOUT_FILENO
#include <stdlib.h>  // for atoi, free
#include <fcntl.h>   // for open, O_WRONLY
#include <stdio.h>   // for dprintf


static int nullfd;  // file descriptor for output

// Helper to parse arguments
static int do_parse(int argc, char **argv, po_args_t *args) {
    po_args_init(args);
    return po_args_parse(args, argc, argv, nullfd); // don't print to screen, use /dev/null
}

TEST_GROUP(ARGV);

TEST_SETUP(ARGV) {
    nullfd = open("/dev/null", O_WRONLY);
    TEST_ASSERT_TRUE(nullfd >= 0);
}

TEST_TEAR_DOWN(ARGV) {
    optind = 1;
    TEST_ASSERT_EQUAL_INT32(0, close(nullfd));
}

TEST(ARGV, DefaultValues) {
    po_args_t args;
    char *argv[] = { "prog" };
    int rc = do_parse(1, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(args.help);
    TEST_ASSERT_FALSE(args.version);
    TEST_ASSERT_NULL(args.config_file);
    TEST_ASSERT_EQUAL_INT(0, args.loglevel);
}

TEST(ARGV, HelpShort) {
    po_args_t args;
    char *argv[] = { "prog", "-h" };
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.help);
}

TEST(ARGV, HelpLong) {
    po_args_t args;
    char *argv[] = { "prog", "--help" };
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.help);
}

TEST(ARGV, VersionShort) {
    po_args_t args;
    char *argv[] = { "prog", "-v" };
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.version);
}

TEST(ARGV, VersionLong) {
    po_args_t args;
    char *argv[] = { "prog", "--version" };
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.version);
}

TEST(ARGV, ConfigOption) {
    po_args_t args;
    char *argv[] = { "prog", "-c", "conf.ini" };
    int rc = do_parse(3, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("conf.ini", args.config_file);
    free(args.config_file);
}

TEST(ARGV, ConfigLong) {
    po_args_t args;
    char *argv[] = { "prog", "--config", "app.ini" };
    int rc = do_parse(3, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("app.ini", args.config_file);
    free(args.config_file);
}

TEST(ARGV, LogLevel) {
    po_args_t args;
    char *argv[] = { "prog", "-l", "2" };
    int rc = do_parse(3, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(2, args.loglevel);
}

TEST_GROUP_RUNNER(ARGV) {
    RUN_TEST_CASE(ARGV, DefaultValues);
    RUN_TEST_CASE(ARGV, HelpShort);
    RUN_TEST_CASE(ARGV, HelpLong);
    RUN_TEST_CASE(ARGV, VersionShort);
    RUN_TEST_CASE(ARGV, VersionLong);
    RUN_TEST_CASE(ARGV, ConfigOption);
    RUN_TEST_CASE(ARGV, ConfigLong);
    RUN_TEST_CASE(ARGV, LogLevel);
}
