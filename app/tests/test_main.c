#include "unity/unity_fixture.h"

// declare all your group runners:
extern TEST_GROUP_RUNNER(Argv);
extern TEST_GROUP_RUNNER(Configs);
// etc for other suites...

static void RunAllTests(void) {
    RUN_TEST_GROUP(Argv);
    RUN_TEST_GROUP(Configs);
    // RUN_TEST_GROUP(OtherSuite);
}

int main(int argc, const char *argv[]) {
    return UnityMain(argc, argv, RunAllTests);
}
