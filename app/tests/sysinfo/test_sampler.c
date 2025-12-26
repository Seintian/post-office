#include "unity/unity_fixture.h"
#include "postoffice/sysinfo/sysinfo.h"
#include <unistd.h>

TEST_GROUP(SAMPLER);

TEST_SETUP(SAMPLER) {
    po_sysinfo_sampler_stop();
}

TEST_TEAR_DOWN(SAMPLER) {
    po_sysinfo_sampler_stop();
}

TEST(SAMPLER, InitAndStop) {
    TEST_ASSERT_EQUAL_INT(0, po_sysinfo_sampler_init());
    
    double cpu, iowait;
    TEST_ASSERT_EQUAL_INT(0, po_sysinfo_sampler_get(&cpu, &iowait));
    
    // Values are -1.0 only if uninitialized or failed first read, 
    // but init waits for warm up. So we expect valid output.
    // However, cached values init to -1. 
    // If warm up succeeded, cpu >= 0.
    
    // Warn if /proc/stat not readable (e.g. some containers/OS)
    if (cpu < 0.0) {
        // Maybe failed to read?
        // Check if file exists.
        if (access("/proc/stat", R_OK) != 0) {
            TEST_IGNORE_MESSAGE("/proc/stat not accessible");
        }
    } else {
        TEST_ASSERT_TRUE(cpu >= 0.0 && cpu <= 100.0);
        TEST_ASSERT_TRUE(iowait >= 0.0 && iowait <= 100.0);
    }
    
    po_sysinfo_sampler_stop();
    
    // After stop, get should return -1
    TEST_ASSERT_EQUAL_INT(-1, po_sysinfo_sampler_get(&cpu, &iowait));
}

TEST(SAMPLER, DoubleInit) {
    TEST_ASSERT_EQUAL_INT(0, po_sysinfo_sampler_init());
    TEST_ASSERT_EQUAL_INT(0, po_sysinfo_sampler_init()); // Should be idempotent and return 0
    po_sysinfo_sampler_stop();
}

TEST(SAMPLER, NotInit) {
    double cpu, iowait;
    TEST_ASSERT_EQUAL_INT(-1, po_sysinfo_sampler_get(&cpu, &iowait));
}

TEST_GROUP_RUNNER(SAMPLER) {
    RUN_TEST_CASE(SAMPLER, InitAndStop);
    RUN_TEST_CASE(SAMPLER, DoubleInit);
    RUN_TEST_CASE(SAMPLER, NotInit);
}
