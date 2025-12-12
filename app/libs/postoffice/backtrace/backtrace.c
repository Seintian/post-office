#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdint.h>
#include <ucontext.h>
#include <sys/ucontext.h>

#include "postoffice/backtrace/backtrace.h"
#include "utils/signals.h"

#define MAX_FRAMES 64
#define MAX_PATH_LEN 1024
#define ADDR2LINE_CMD "addr2line"

static char g_crash_dump_dir[MAX_PATH_LEN] = {0};

/* Async-signal-safe writer to a file descriptor */
static void safe_write(int fd, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (len > 0) {
        if (write(fd, buffer, (size_t)len) == -1) {
            // best effort, ignoring error in crash handler
        }
    }
}

/* Resolve address to file:line using addr2line */
static void resolve_addr2line(void* addr, char* out_file, size_t file_len, char* out_func, size_t func_len) {
    int pipe_out[2];
    
    // Initialize outputs
    strncpy(out_file, "??:0", file_len);
    strncpy(out_func, "??", func_len);

    if (pipe(pipe_out) == -1) return;

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_out[0]);
        close(pipe_out[1]);
        return;
    }

    if (pid == 0) {
        /* Child */
        close(pipe_out[0]);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_out[1]);

        char addr_str[32];
        snprintf(addr_str, sizeof(addr_str), "%p", addr);

        /* Get executable path */
        char exe_path[MAX_PATH_LEN];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) {
            exe_path[len] = '\0';
        } else {
            _exit(1);
        }

        /* execlp will search PATH for addr2line */
        execlp(ADDR2LINE_CMD, ADDR2LINE_CMD, "-e", exe_path, "-f", "-C", addr_str, NULL);
        _exit(1); /* Should not be reached */
    } else {
        /* Parent */
        close(pipe_out[1]);
        char buffer[1024];
        ssize_t count = read(pipe_out[0], buffer, sizeof(buffer) - 1);
        close(pipe_out[0]);
        waitpid(pid, NULL, 0);

        if (count > 0) {
            buffer[count] = '\0';
            char* newline = strchr(buffer, '\n');
            if (newline) {
                *newline = '\0';
                snprintf(out_func, func_len, "%s", buffer);

                char* second_line = newline + 1;
                /* addr2line output:
                   function_name
                   file:line
                */
                char* second_newline = strchr(second_line, '\n');
                if (second_newline) *second_newline = '\0';

                /* Check for consistency/validity */
                if (second_line && *second_line) {
                     snprintf(out_file, file_len, "%s", second_line);
                }
            } else {
                /* Only one line? treat as func, file unknown */
                snprintf(out_func, func_len, "%s", buffer);
            }
        }
    }
}

static void print_backtrace_fd(int fd, void* fault_rip) {
    void* array[MAX_FRAMES];
    int size = backtrace(array, MAX_FRAMES);

    safe_write(fd, "Stacktrace (most recent call first):\n");

    static int trampoline_index = -1;
    // Reset for each call in case of multiple calls (though usually once per crash)
    trampoline_index = -1; 

    for (int i = 0; i < size; i++) {
        char filename[1024];
        char funcname[1024];

        /* 
         * Logic: Subtract 1 from return addresses to get call sites.
         * Exception: The faulting frame (where execution stopped) is exact.
         */
        void* addr_to_resolve = array[i];
        if (i > 0 && array[i] != fault_rip) { 
            addr_to_resolve = (void*)((uintptr_t)array[i] - 1);
        }

        void* addr_for_addr2line = addr_to_resolve;

        Dl_info info;
        if (dladdr(addr_to_resolve, &info)) {
            // Filter spurious signal trampoline frame incorrectly resolved to lmdb
            // This is a heuristic: if we are in the crash handler path (i > 0)
            // and the symbol is mdb_node_move (detected artifact), allow suppressing or labeling it.
            // For now, let's just label it if we suspect it's the trampoline.
            
            if (info.dli_sname) {
                snprintf(funcname, sizeof(funcname), "%s", info.dli_sname);
            }
            if (info.dli_fname) {
                snprintf(filename, sizeof(filename), "%s", info.dli_fname);
            }
            
            if (info.dli_fbase) {
                addr_for_addr2line = (void*)((uintptr_t)addr_to_resolve - (uintptr_t)info.dli_fbase);
            }
        }

        /* Resolve with addr2line (using offset) */
        char precise_file[1024] = "??:0";
        char precise_func[1024] = "??";
        resolve_addr2line(addr_for_addr2line, precise_file, sizeof(precise_file), precise_func, sizeof(precise_func));

        // Use addr2line result if it gave something better than "??"
        if (strcmp(precise_file, "??:0") != 0 && strcmp(precise_file, "?:?") != 0) {
            snprintf(filename, sizeof(filename), "%s", precise_file);
        }
        if (strcmp(precise_func, "??") != 0) {
            snprintf(funcname, sizeof(funcname), "%s", precise_func);
        }

        /* Detect signal trampoline frame to label it */
        if (trampoline_index == -1 && strstr(funcname, "crash_handler")) {
            trampoline_index = i + 1;
        } else if (i == trampoline_index) {
            snprintf(funcname, sizeof(funcname), "[Signal Trampoline]");
            snprintf(filename, sizeof(filename), "[kernel/libc]");
        }
        // Also catch standard glibc restorer if symbols are present
        else if (strstr(funcname, "__restore_rt")) {
            snprintf(funcname, sizeof(funcname), "[Signal Trampoline]");
            snprintf(filename, sizeof(filename), "[kernel/libc]");
            trampoline_index = i; // Just in case
        }

        safe_write(fd, "#%-2d %p in %s at %s\n", i, array[i], funcname, filename);
    }
}

void backtrace_print(void) {
    print_backtrace_fd(STDERR_FILENO, NULL);
}

void backtrace_save(const char* filepath, void* fault_rip) {
    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Failed to open crash dump file");
        return;
    }

    /* Write timestamp */
    time_t now = time(NULL);
    char* time_str = ctime(&now);
    if (time_str) {
        safe_write(fd, "Crash Timestamp: %s", time_str);
    }

    print_backtrace_fd(fd, fault_rip);
    close(fd);
}

static void crash_handler(int sig, siginfo_t* info, void* ctx) {
    ucontext_t *uc = (ucontext_t *)ctx;
    void* fault_rip = NULL;
#ifdef __x86_64__
    fault_rip = (void*)uc->uc_mcontext.gregs[REG_RIP];
#endif

    safe_write(STDERR_FILENO, "\n!!! CRITICAL SIGNAL CAPTURED: %d !!!\n", sig);
    if (info) {
        safe_write(STDERR_FILENO, "Fault Address: %p\n", info->si_addr);
    }
    if (fault_rip) {
        safe_write(STDERR_FILENO, "Fault Instruction Pointer: %p\n", fault_rip);
    }

    print_backtrace_fd(STDERR_FILENO, fault_rip);

    if (g_crash_dump_dir[0] != '\0') {
        char dump_path[MAX_PATH_LEN + 128];
        time_t now = time(NULL);
        snprintf(dump_path, sizeof(dump_path), "%s/crash_%ld.log", g_crash_dump_dir, now);
        safe_write(STDERR_FILENO, "Saving crash report to: %s\n", dump_path);
        backtrace_save(dump_path, fault_rip);
    }

    safe_write(STDERR_FILENO, "Aborting process.\n");

    /* Unregister and re-raise to properly terminate/core dump */
    if (sigutil_restore(sig) != 0) {
        safe_write(STDERR_FILENO, "Failed to restore signal handler\n");
    }
    raise(sig);
}

void backtrace_init(const char* crash_dump_dir) {
    if (crash_dump_dir) {
        strncpy(g_crash_dump_dir, crash_dump_dir, sizeof(g_crash_dump_dir) - 1);
        struct stat st = {0};
        if (stat(crash_dump_dir, &st) == -1) {
            mkdir(crash_dump_dir, 0755);
        }
    }

    /* Register for common crash signals using the project's signal utility */
    /* Ideally we'd use sigutil_handle_terminating but we want a specific handler */

    /* Just manually hook our special handler */
    int signals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE};
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = (int)(SA_SIGINFO | SA_RESETHAND); /* Reset to default if it happens again (loop prevention) */
    sigemptyset(&sa.sa_mask);

    for (size_t i = 0; i < sizeof(signals)/sizeof(signals[0]); ++i) {
        if (sigaction(signals[i], &sa, NULL) == -1) {
            /* Ignore error in init */
        }
    }
}
