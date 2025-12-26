#include "unity/unity_fixture.h"
#include "utils/signals.h"
#include <signal.h>
#include <stdbool.h>

TEST_GROUP(SIGNALS);

static volatile bool handler_called = false;
static int received_sig = 0;

static void test_signal_handler(int sig, siginfo_t *info, void *context) {
    (void)info;
    (void)context;
    handler_called = true;
    received_sig = sig;
}

TEST_SETUP(SIGNALS) {
    handler_called = false;
    received_sig = 0;
    TEST_ASSERT_EQUAL_INT(0, sigutil_restore_all());
}

TEST_TEAR_DOWN(SIGNALS) {
    TEST_ASSERT_EQUAL_INT(0, sigutil_restore_all());
}

TEST(SIGNALS, BlockAndUnblock) {
    sigset_t set;
    
    // Ensure SIGUSR1 is not blocked initially (or restore clears it)
    TEST_ASSERT_EQUAL_INT(0, sigutil_get_blocked_signals(&set));
    if (sigismember(&set, SIGUSR1)) {
        TEST_ASSERT_EQUAL_INT(0, sigutil_unblock(SIGUSR1));
    }

    // Block
    TEST_ASSERT_EQUAL_INT(0, sigutil_block(SIGUSR1));
    
    // Verify
    TEST_ASSERT_EQUAL_INT(0, sigutil_get_blocked_signals(&set));
    TEST_ASSERT_TRUE(sigismember(&set, SIGUSR1));

    // Unblock
    TEST_ASSERT_EQUAL_INT(0, sigutil_unblock(SIGUSR1));

    // Verify
    TEST_ASSERT_EQUAL_INT(0, sigutil_get_blocked_signals(&set));
    TEST_ASSERT_FALSE(sigismember(&set, SIGUSR1));
}

TEST(SIGNALS, HandleSignal) {
    TEST_ASSERT_EQUAL_INT(0, sigutil_handle(SIGUSR1, test_signal_handler, 0));
    
    TEST_ASSERT_FALSE(handler_called);
    raise(SIGUSR1);
    TEST_ASSERT_TRUE(handler_called);
    TEST_ASSERT_EQUAL_INT(SIGUSR1, received_sig);
}

TEST(SIGNALS, HandleAll) {
    // Skipped
}

TEST(SIGNALS, BlockTerminating) {
    TEST_ASSERT_EQUAL_INT(0, sigutil_block_terminating());
    
    sigset_t set;
    TEST_ASSERT_EQUAL_INT(0, sigutil_get_blocked_signals(&set));
    
    TEST_ASSERT_TRUE(sigismember(&set, SIGTERM));
    TEST_ASSERT_TRUE(sigismember(&set, SIGINT));
}

TEST_GROUP_RUNNER(SIGNALS) {
    RUN_TEST_CASE(SIGNALS, BlockAndUnblock);
    RUN_TEST_CASE(SIGNALS, HandleSignal);
    RUN_TEST_CASE(SIGNALS, BlockTerminating);
}
