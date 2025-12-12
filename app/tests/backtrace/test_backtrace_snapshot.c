#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "unity/unity_fixture.h"
#include "postoffice/backtrace/backtrace.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdint.h>
#include <signal.h>
#include <fcntl.h>

TEST_GROUP(BACKTRACE);

TEST_SETUP(BACKTRACE) {
}

TEST_TEAR_DOWN(BACKTRACE) {
}

static void recursive_delete(const char* dir) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
    if (system(cmd) != 0) { /* ignore */ }
}

TEST(BACKTRACE, GeneratesSnapshot) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        TEST_FAIL_MESSAGE("Failed to get CWD");
    }

    char dump_dir[2048];
    snprintf(dump_dir, sizeof(dump_dir), "%s/test_crash_dumps", cwd);

    // Ensure clean state
    recursive_delete(dump_dir);
    mkdir(dump_dir, 0755);

    pid_t pid = fork();
    if (pid == -1) {
        TEST_FAIL_MESSAGE("Fork failed");
    }

    if (pid == 0) {
        // Child process
        backtrace_init(dump_dir);

        // Put something distinctive in a register (like RBX)
#ifdef __x86_64__
        asm volatile("mov $0x1234567890ABCDEF, %rbx");
#endif

        // Trigger crash
        volatile uintptr_t bad_addr = 0;
        *(volatile int*)bad_addr = 0;

        _exit(1);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        // Verify file was created
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "ls %s/crash_*.log", dump_dir);

        FILE *fp = popen(cmd, "r");
        if (!fp) {
            TEST_FAIL_MESSAGE("Failed to run ls command");
        }

        char path[1024];
        if (fgets(path, sizeof(path), fp) == NULL) {
            pclose(fp);
            TEST_FAIL_MESSAGE("No crash log found");
            return;
        }

        size_t len = strlen(path);
        if (len > 0 && path[len-1] == '\n') path[len-1] = '\0';
        pclose(fp);

        // Read file content
        FILE *log = fopen(path, "r");
        TEST_ASSERT_NOT_NULL(log);

        fseek(log, 0, SEEK_END);
        long length_long = ftell(log);
        fseek(log, 0, SEEK_SET);

        size_t length = (size_t)length_long;
        char *content = malloc(length + 1);
        if (fread(content, 1, length, log) != length) {
            TEST_FAIL_MESSAGE("Failed to read full log file");
        }
        content[length] = '\0';
        fclose(log);

        // Verify Content
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "Stacktrace (most recent call first):"), "Missing 'Stacktrace' section");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "Registers:"), "Missing 'Registers' section");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "Stack Dump (SP +/- 256 bytes):"), "Missing 'Stack Dump' section");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "Memory Maps:"), "Missing 'Memory Maps' section");

        // Extended Sections
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "Signal Details:"), "Missing 'Signal Details' section");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "Signal: 11"), "Missing Signal 11 confirmation");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "Command Line:"), "Missing 'Command Line' section");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "Process Status (/proc/self/status):"), "Missing 'Process Status' section");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "Open File Descriptors:"), "Missing 'Open File Descriptors' section");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "Environment Variables:"), "Missing 'Environment Variables' section");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "--- Pending Log Messages (Ring Buffer Dump) ---"), "Missing 'Pending Log Messages' section");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "--- End of Pending Logs ---"), "Missing 'End of Pending Logs' marker");

#ifdef __x86_64__
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "RBX: 1234567890abcdef"), "Missing or incorrect RBX canary value");
#endif

        free(content);
        recursive_delete(dump_dir);
    }
}

TEST_GROUP_RUNNER(BACKTRACE) {
    RUN_TEST_CASE(BACKTRACE, GeneratesSnapshot);
}
