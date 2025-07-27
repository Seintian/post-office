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

TEST_GROUP(Argv);

TEST_SETUP(Argv) {
    nullfd = open("/dev/null", O_WRONLY);
    TEST_ASSERT_TRUE(nullfd >= 0);
}

TEST_TEAR_DOWN(Argv) {
    optind = 1;
    TEST_ASSERT_EQUAL_INT32(0, close(nullfd));
}

TEST(Argv, DefaultValues) {
    po_args_t args;
    char *argv[] = { "prog" };
    int rc = do_parse(1, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(args.help);
    TEST_ASSERT_FALSE(args.version);
    TEST_ASSERT_NULL(args.config_file);
    TEST_ASSERT_EQUAL_INT(0, args.loglevel);
}

TEST(Argv, HelpShort) {
    po_args_t args;
    char *argv[] = { "prog", "-h" };
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.help);
}

TEST(Argv, HelpLong) {
    po_args_t args;
    char *argv[] = { "prog", "--help" };
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.help);
}

TEST(Argv, VersionShort) {
    po_args_t args;
    char *argv[] = { "prog", "-v" };
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.version);
}

TEST(Argv, VersionLong) {
    po_args_t args;
    char *argv[] = { "prog", "--version" };
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.version);
}

TEST(Argv, ConfigOption) {
    po_args_t args;
    char *argv[] = { "prog", "-c", "conf.ini" };
    int rc = do_parse(3, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("conf.ini", args.config_file);
    free(args.config_file);
}

TEST(Argv, ConfigLong) {
    po_args_t args;
    char *argv[] = { "prog", "--config", "app.ini" };
    int rc = do_parse(3, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("app.ini", args.config_file);
    free(args.config_file);
}

TEST(Argv, LogLevel) {
    po_args_t args;
    char *argv[] = { "prog", "-l", "2" };
    int rc = do_parse(3, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(2, args.loglevel);
}

TEST_GROUP_RUNNER(Argv) {
    RUN_TEST_CASE(Argv, DefaultValues);
    RUN_TEST_CASE(Argv, HelpShort);
    RUN_TEST_CASE(Argv, HelpLong);
    RUN_TEST_CASE(Argv, VersionShort);
    RUN_TEST_CASE(Argv, VersionLong);
    RUN_TEST_CASE(Argv, ConfigOption);
    RUN_TEST_CASE(Argv, ConfigLong);
    RUN_TEST_CASE(Argv, LogLevel);
}
