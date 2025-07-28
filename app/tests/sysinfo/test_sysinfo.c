#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "unity/unity_fixture.h"
#include "core/simulation/sysinfo.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>


TEST_GROUP(SysInfo);

TEST_SETUP(SysInfo) {
    // Initialize any necessary resources or state before each test
}

TEST_TEAR_DOWN(SysInfo) {
    // Clean up any resources or state after each test
}

TEST(SysInfo, TestSysInfoCollectSuccess) {
    po_sysinfo_t info;
    memset(&info, 0, sizeof(info));

    // Collect system information
    int result = po_sysinfo_collect(&info);
    TEST_ASSERT_EQUAL_INT(0, result);

    // Validate collected data
    TEST_ASSERT(info.physical_cores > 0);
    TEST_ASSERT(info.logical_processors > 0);
    TEST_ASSERT(info.total_ram > 0);
    TEST_ASSERT(info.free_ram >= 0);
    TEST_ASSERT(info.page_size > 0);
    TEST_ASSERT(info.max_open_files > 0);
    TEST_ASSERT(info.max_processes > 0);
    TEST_ASSERT(info.max_stack_size > 0);
}

TEST(SysInfo, TestSysInfoCollectInvalidPointer) {
    // Attempt to collect system information with a NULL pointer
    int result = po_sysinfo_collect(NULL);
    TEST_ASSERT_EQUAL_INT(-1, result);
    TEST_ASSERT_EQUAL_INT(EINVAL, errno); // Check for invalid argument error
}

/* --------------------------------------------------------------------------
 * Helper to call po_sysinfo_print() into a memory buffer and then
 * assert that it succeeded (no ferror) and actually produced output.
 * -------------------------------------------------------------------------- */
static void assert_print_success(const po_sysinfo_t *info)
{
    char buf[1024];
    memset(buf, 0, sizeof(buf));

    // open a FILE* over our buffer
    FILE *mem = fmemopen(buf, sizeof(buf), "w");
    TEST_ASSERT_NOT_NULL_MESSAGE(mem, "fmemopen failed");

    // call the print function
    po_sysinfo_print(info, mem);

    // make sure everything is flushed
    fflush(mem);

    // 1) no I/O error
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ferror(mem),
        "I/O error during po_sysinfo_print");

    // 2) we actually wrote *something*
    TEST_ASSERT_TRUE_MESSAGE(strlen(buf) > 0,
        "po_sysinfo_print produced empty output");

    fclose(mem);
}

TEST(SysInfo, TestSysInfoPrint) {
    po_sysinfo_t info;
    memset(&info, 0, sizeof(info));

    // collect must succeed
    TEST_ASSERT_EQUAL_INT(0, po_sysinfo_collect(&info));

    // now verify that print() actually printed
    assert_print_success(&info);
}

TEST_GROUP_RUNNER(SysInfo) {
    RUN_TEST_CASE(SysInfo, TestSysInfoCollectSuccess);
    RUN_TEST_CASE(SysInfo, TestSysInfoCollectInvalidPointer);
    RUN_TEST_CASE(SysInfo, TestSysInfoPrint);
}
