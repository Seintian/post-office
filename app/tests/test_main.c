#include "unity/unity_fixture.h"

// declare all your group runners:
extern TEST_GROUP_RUNNER(Argv);
extern TEST_GROUP_RUNNER(Configs);
extern TEST_GROUP_RUNNER(SysInfo);
extern TEST_GROUP_RUNNER(Perf);
extern TEST_GROUP_RUNNER(HashTable);
extern TEST_GROUP_RUNNER(HashSet);
// etc for other suites...

static void RunAllTests(void) {
    RUN_TEST_GROUP(Argv);
    RUN_TEST_GROUP(Configs);
    RUN_TEST_GROUP(SysInfo);
    RUN_TEST_GROUP(Perf);
    RUN_TEST_GROUP(HashTable);
    RUN_TEST_GROUP(HashSet);
}

int main(int argc, const char *argv[]) {
    return UnityMain(argc, argv, RunAllTests);
}
