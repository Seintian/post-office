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
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>

TEST_GROUP(BACKTRACE);

TEST_SETUP(BACKTRACE) {
}

TEST_TEAR_DOWN(BACKTRACE) {
}

#include <ftw.h>

static int remove_callback(const char *fpath, const struct stat *sb, 
                          int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)typeflag;
    (void)ftwbuf;
    return remove(fpath);
}

static void recursive_delete(const char* dir) {
    nftw(dir, remove_callback, 64, FTW_DEPTH | FTW_PHYS);
}

TEST(BACKTRACE, GENERATES_SNAPSHOT) {
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
        // Suppress stderr to keep test output clean
        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) {
            dup2(nullfd, STDERR_FILENO);
            close(nullfd);
        }

        backtrace_init(dump_dir);

        // Put something distinctive in a register for verification
#if defined(__x86_64__)
        asm volatile("mov $0x1234567890ABCDEF, %rbx");
#elif defined(__i386__)
        asm volatile("mov $0x12345678, %ebx");
#elif defined(__aarch64__)
        register uint64_t x19 asm("x19") = 0x1234567890ABCDEF;
        asm volatile("" :: "r"(x19));
#elif defined(__arm__)
        register uint32_t r4 asm("r4") = 0x12345678;
        asm volatile("" :: "r"(r4));
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
    DIR *d = opendir(dump_dir);
    if (!d) {
        TEST_FAIL_MESSAGE("Failed to open dump directory");
    }

    char path[4096];
    struct dirent *entry;
    int found = 0;
    while ((entry = readdir(d)) != NULL) {
        if (strncmp(entry->d_name, "crash_", 6) == 0 && 
            strstr(entry->d_name, ".log") != NULL) {
            snprintf(path, sizeof(path), "%s/%s", dump_dir, entry->d_name);
            found = 1;
            break;
        }
    }
    closedir(d);

    if (!found) {
        TEST_FAIL_MESSAGE("No crash log found");
    }

    // Read file content
    FILE *log = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(log);

    fseek(log, 0, SEEK_END);
    long length_long = ftell(log);
    if (length_long < 0) {
        fclose(log);
        TEST_FAIL_MESSAGE("ftell failed");
    }
    fseek(log, 0, SEEK_SET);

    size_t length = (size_t)length_long;
    char *content = malloc(length + 1);
    if (!content) {
        fclose(log);
        TEST_FAIL_MESSAGE("malloc failed");
    }

    if (fread(content, 1, length, log) != length) {
        free(content);
        fclose(log);
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

#if defined(__x86_64__)
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "RBX: 1234567890abcdef"), "Missing or incorrect RBX canary value");
#elif defined(__i386__)
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "EBX: 12345678"), "Missing or incorrect EBX canary value");
#elif defined(__aarch64__)
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "X19: 1234567890abcdef"), "Missing or incorrect X19 canary value");
#elif defined(__arm__)
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, "R4 : 12345678"), "Missing or incorrect R4 canary value");
#endif

        free(content);
        recursive_delete(dump_dir);
    }
}

TEST_GROUP_RUNNER(BACKTRACE) {
    RUN_TEST_CASE(BACKTRACE, GENERATES_SNAPSHOT);
}
