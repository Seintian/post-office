#include <fcntl.h>  // for open, O_WRONLY
#include <getopt.h> // for optind
#include <stdio.h>  // for dprintf
#include <stdlib.h> // for atoi, free
#include <unistd.h> // for STDOUT_FILENO

#include "unity/unity_fixture.h"
#include "utils/argv.h"

static int nullfd; // file descriptor for output

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

TEST(ARGV, DEFAULT_VALUES) {
    po_args_t args;
    char *argv[] = {"prog"};
    int rc = do_parse(1, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_FALSE(args.help);
    TEST_ASSERT_FALSE(args.version);
    TEST_ASSERT_NULL(args.config_file);
    TEST_ASSERT_EQUAL_INT(2, args.loglevel);
}

TEST(ARGV, HELP_SHORT) {
    po_args_t args;
    char *argv[] = {"prog", "-h"};
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.help);
}

TEST(ARGV, HELP_LONG) {
    po_args_t args;
    char *argv[] = {"prog", "--help"};
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.help);
}

TEST(ARGV, VERSION_SHORT) {
    po_args_t args;
    char *argv[] = {"prog", "-v"};
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.version);
}

TEST(ARGV, VERSION_LONG) {
    po_args_t args;
    char *argv[] = {"prog", "--version"};
    int rc = do_parse(2, argv, &args);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_TRUE(args.version);
}

TEST(ARGV, CONFIG_OPTION) {
    po_args_t args;
    char *argv[] = {"prog", "-c", "conf.ini"};
    int rc = do_parse(3, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("conf.ini", args.config_file);
    free(args.config_file);
}

TEST(ARGV, CONFIG_LONG) {
    po_args_t args;
    char *argv[] = {"prog", "--config", "app.ini"};
    int rc = do_parse(3, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("app.ini", args.config_file);
    free(args.config_file);
}

TEST(ARGV, LOG_LEVEL) {
    po_args_t args;
    char *argv[] = {"prog", "-l", "2"};
    int rc = do_parse(3, argv, &args);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(2, args.loglevel);
}

TEST_GROUP_RUNNER(ARGV) {
    RUN_TEST_CASE(ARGV, DEFAULT_VALUES);
    RUN_TEST_CASE(ARGV, HELP_SHORT);
    RUN_TEST_CASE(ARGV, HELP_LONG);
    RUN_TEST_CASE(ARGV, VERSION_SHORT);
    RUN_TEST_CASE(ARGV, VERSION_LONG);
    RUN_TEST_CASE(ARGV, CONFIG_OPTION);
    RUN_TEST_CASE(ARGV, CONFIG_LONG);
    RUN_TEST_CASE(ARGV, LOG_LEVEL);
}
