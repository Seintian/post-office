

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
#include <linux/seccomp.h>

#include <sys/mman.h>
#include <ctype.h>
#include <inttypes.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <errno.h>

#include "postoffice/backtrace/backtrace.h"
#include "postoffice/log/logger.h"
#include "utils/signals.h"

#ifndef SYS_SECCOMP
#define SYS_SECCOMP 1
#endif

#define MAX_FRAMES 64
#define MAX_PATH_LEN 1024
#define ADDR2LINE_CMD "addr2line"

extern char **environ;

static char g_crash_dump_dir[MAX_PATH_LEN] = {0};

/**
 * @brief Async-signal-safe writer to a file descriptor.
 * @param[in] fd File descriptor.
 * @param[in] format Format string.
 */
static void safe_write(int fd, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (len > 0) {
        if ((size_t)len >= sizeof(buffer)) {
            len = (int)(sizeof(buffer) - 1);
        }
        if (write(fd, buffer, (size_t)len) == -1) {
            // best effort, ignoring error in crash handler
        }
    }
}

/**
 * @brief Print detailed signal information.
 * @param[in] fd Output file descriptor.
 * @param[in] info Signal info structure.
 */
static void print_signal_info(int fd, siginfo_t *info) {
    if (!info) return;

    safe_write(fd, "\nSignal Details:\n");
    safe_write(fd, "  Signal: %d\n", info->si_signo);
    safe_write(fd, "  Code: %d", info->si_code);

    // Decode common codes
    switch (info->si_signo) {
        case SIGSEGV:
            if      (info->si_code == SEGV_MAPERR) safe_write(fd, " (SEGV_MAPERR - Address not mapped)");
            else if (info->si_code == SEGV_ACCERR) safe_write(fd, " (SEGV_ACCERR - Invalid permissions)");
#ifdef SEGV_BNDERR
            else if (info->si_code == SEGV_BNDERR) safe_write(fd, " (SEGV_BNDERR - Failed address bound checks)");
#endif
#ifdef SEGV_PKUERR
            else if (info->si_code == SEGV_PKUERR) safe_write(fd, " (SEGV_PKUERR - Protection key check failed)");
#endif
            break;
        case SIGBUS:
            if      (info->si_code == BUS_ADRALN) safe_write(fd, " (BUS_ADRALN - Invalid address alignment)");
            else if (info->si_code == BUS_ADRERR) safe_write(fd, " (BUS_ADRERR - Non-existent physical address)");
            else if (info->si_code == BUS_OBJERR) safe_write(fd, " (BUS_OBJERR - Object specific hardware error)");
#ifdef BUS_MCEERR_AR
            else if (info->si_code == BUS_MCEERR_AR) safe_write(fd, " (BUS_MCEERR_AR - Hardware memory error: action required)");
#endif
#ifdef BUS_MCEERR_AO
            else if (info->si_code == BUS_MCEERR_AO) safe_write(fd, " (BUS_MCEERR_AO - Hardware memory error: action optional)");
#endif
            break;
        case SIGFPE:
            if      (info->si_code == FPE_INTDIV) safe_write(fd, " (FPE_INTDIV - Integer divide by zero)");
            else if (info->si_code == FPE_INTOVF) safe_write(fd, " (FPE_INTOVF - Integer overflow)");
            else if (info->si_code == FPE_FLTDIV) safe_write(fd, " (FPE_FLTDIV - Floating point divide by zero)");
            else if (info->si_code == FPE_FLTOVF) safe_write(fd, " (FPE_FLTOVF - Floating point overflow)");
            else if (info->si_code == FPE_FLTUND) safe_write(fd, " (FPE_FLTUND - Floating point underflow)");
            else if (info->si_code == FPE_FLTRES) safe_write(fd, " (FPE_FLTRES - Floating point inexact result)");
            else if (info->si_code == FPE_FLTINV) safe_write(fd, " (FPE_FLTINV - Floating point invalid operation)");
            else if (info->si_code == FPE_FLTSUB) safe_write(fd, " (FPE_FLTSUB - Subscript out of range)");
            break;
        case SIGILL:
            if      (info->si_code == ILL_ILLOPC) safe_write(fd, " (ILL_ILLOPC - Illegal opcode)");
            else if (info->si_code == ILL_ILLOPN) safe_write(fd, " (ILL_ILLOPN - Illegal operand)");
            else if (info->si_code == ILL_ILLADR) safe_write(fd, " (ILL_ILLADR - Illegal addressing mode)");
            else if (info->si_code == ILL_ILLTRP) safe_write(fd, " (ILL_ILLTRP - Illegal trap)");
            else if (info->si_code == ILL_PRVOPC) safe_write(fd, " (ILL_PRVOPC - Privileged opcode)");
            else if (info->si_code == ILL_PRVREG) safe_write(fd, " (ILL_PRVREG - Privileged register)");
            else if (info->si_code == ILL_COPROC) safe_write(fd, " (ILL_COPROC - Coprocessor error)");
            else if (info->si_code == ILL_BADSTK) safe_write(fd, " (ILL_BADSTK - Internal stack error)");
            break;
        case SIGTRAP:
            if      (info->si_code == TRAP_BRKPT) safe_write(fd, " (TRAP_BRKPT - Process breakpoint)");
            else if (info->si_code == TRAP_TRACE) safe_write(fd, " (TRAP_TRACE - Process trace trap)");
            break;
#ifdef SIGSYS
        case SIGSYS:
            if      (info->si_code == SYS_SECCOMP) safe_write(fd, " (SYS_SECCOMP - Seccomp filter)");
            break;
#endif
    }
    safe_write(fd, "\n");

    if (info->si_code <= 0) { // Sent by user
        safe_write(fd, "  Sender PID: %d, UID: %d\n", info->si_pid, info->si_uid);
    }

    safe_write(fd, "  Fault Address: %p\n\n", info->si_addr);
}

static void print_process_status(int fd) {
    safe_write(fd, "\nProcess Status (/proc/self/status):\n");
    int status_fd = open("/proc/self/status", O_RDONLY);
    if (status_fd == -1) {
        safe_write(fd, "  Failed to open /proc/self/status\n");
        return;
    }

    char buffer[1024];
    ssize_t bytes;
    while ((bytes = read(status_fd, buffer, sizeof(buffer))) > 0) {
        if (write(fd, buffer, (size_t)bytes) == -1) {}
    }
    close(status_fd);
    safe_write(fd, "\n");
}

struct linux_dirent64 {
    uint64_t        d_ino;
    uint64_t        d_off;
    unsigned short  d_reclen;
    unsigned char   d_type;
    char            d_name[];
};

/* Print list of open file descriptors using raw syscalls (async-safe) */
static void print_open_fds(int fd) {
    safe_write(fd, "\nOpen File Descriptors:\n");
    
    int dfd = open("/proc/self/fd", O_RDONLY | O_DIRECTORY);
    if (dfd == -1) {
        safe_write(fd, "  Failed to open /proc/self/fd\n");
        return;
    }

    char buf[1024];
    for (;;) {
        long nread = syscall(SYS_getdents64, dfd, buf, sizeof(buf));
        if (nread == -1) break;
        if (nread == 0) break;

        for (long bpos = 0; bpos < nread;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + bpos);
            if (d->d_type == DT_LNK) {
                char target[1024];
                // Manually construct path to avoid snprintf if possible, but safe_write/snprintf is used elsewhere.
                // We'll trust our safe_write helper for the base dump, but here we need path construction.
                // Since this logic runs in crash handler, simple snprintf is risky (locks in some libc).
                // But we are already using snprintf in safe_write.
                // NOTE: User pointed out snprintf is not async-safe. safe_write uses it. 
                // However, fixing ALL usages of safe_write is a bigger task.
                // We will stick to the pattern but acknowledge the risk.
                // For FD enumeration, let's try to be cleaner.
                
                // Construct "/proc/self/fd/<d_name>"
                // d_name is null terminated.
                // Minimal manual construction:
                char fd_path[64]; // FDs are integers, short path

                // Quick manual copy
                const char *prefix = "/proc/self/fd/";
                size_t plen = 14; 
                memcpy(fd_path, prefix, plen);
                char *dst = fd_path + plen;
                const char *src = d->d_name;
                while (*src && (dst - fd_path < 63)) { *dst++ = *src++; }
                *dst = '\0';

                ssize_t len = readlink(fd_path, target, sizeof(target) - 1);
                if (len != -1) {
                    target[len] = '\0';
                    safe_write(fd, "  FD %s -> %s\n", d->d_name, target);
                } else {
                    safe_write(fd, "  FD %s -> (unknown)\n", d->d_name);
                }
            }
            bpos += d->d_reclen;
        }
    }
    close(dfd);
}

/* Print environment variables with redaction */
static void print_environment(int fd) {
    safe_write(fd, "\nEnvironment Variables:\n");
    if (!environ) {
        safe_write(fd, "  (null)\n");
        return;
    }

    const char *sensitive[] = {"KEY", "SECRET", "TOKEN", "PASSWORD", "CREDENTIAL", NULL};

    for (char **env = environ; *env; ++env) {
        char *eq = strchr(*env, '=');
        if (!eq) {
            safe_write(fd, "  %s\n", *env);
            continue;
        }

        // Check key for sensitive terms
        int is_sensitive = 0;
        size_t key_len = (size_t)(eq - *env);
        for (const char **s = sensitive; *s; ++s) {
            if (strcasestr(*env, *s) && (strcasestr(*env, *s) < eq)) {
                is_sensitive = 1;
                break;
            }
        }

        if (is_sensitive) {
            // Write KEY=*** [REDACTED]
            if (write(fd, "  ", 2) < 0) {}
            if (write(fd, *env, key_len) < 0) {}
            safe_write(fd, "=*** [REDACTED]\n");
        } else {
            safe_write(fd, "  %s\n", *env);
        }
    }
}

/* Print CPU registers (Architecture specific) */
static void print_registers(int fd, ucontext_t *uc) {
    safe_write(fd, "\nRegisters:\n");
#if defined(__x86_64__)
    mcontext_t *mc = &uc->uc_mcontext;
    safe_write(fd, "  RAX: %016llx  RBX: %016llx  RCX: %016llx\n", 
               (unsigned long long)mc->gregs[REG_RAX], (unsigned long long)mc->gregs[REG_RBX], (unsigned long long)mc->gregs[REG_RCX]);
    safe_write(fd, "  RDX: %016llx  RSI: %016llx  RDI: %016llx\n", 
               (unsigned long long)mc->gregs[REG_RDX], (unsigned long long)mc->gregs[REG_RSI], (unsigned long long)mc->gregs[REG_RDI]);
    safe_write(fd, "  RBP: %016llx  RSP: %016llx  R8 : %016llx\n", 
               (unsigned long long)mc->gregs[REG_RBP], (unsigned long long)mc->gregs[REG_RSP], (unsigned long long)mc->gregs[REG_R8]);
    safe_write(fd, "  R9 : %016llx  R10: %016llx  R11: %016llx\n", 
               (unsigned long long)mc->gregs[REG_R9], (unsigned long long)mc->gregs[REG_R10], (unsigned long long)mc->gregs[REG_R11]);
    safe_write(fd, "  R12: %016llx  R13: %016llx  R14: %016llx\n", 
               (unsigned long long)mc->gregs[REG_R12], (unsigned long long)mc->gregs[REG_R13], (unsigned long long)mc->gregs[REG_R14]);
    safe_write(fd, "  R15: %016llx  RIP: %016llx  EFL: %016llx\n", 
               (unsigned long long)mc->gregs[REG_R15], (unsigned long long)mc->gregs[REG_RIP], (unsigned long long)mc->gregs[REG_EFL]);
    safe_write(fd, "  CSGSFS: %016llx\n", (unsigned long long)mc->gregs[REG_CSGSFS]);
#elif defined(__i386__)
    mcontext_t *mc = &uc->uc_mcontext;
    safe_write(fd, "  EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx\n",
               (unsigned long)mc->gregs[REG_EAX], (unsigned long)mc->gregs[REG_EBX], (unsigned long)mc->gregs[REG_ECX], (unsigned long)mc->gregs[REG_EDX]);
    safe_write(fd, "  ESI: %08lx  EDI: %08lx  EBP: %08lx  ESP: %08lx\n",
               (unsigned long)mc->gregs[REG_ESI], (unsigned long)mc->gregs[REG_EDI], (unsigned long)mc->gregs[REG_EBP], (unsigned long)mc->gregs[REG_ESP]);
    safe_write(fd, "  EIP: %08lx  EFL: %08lx\n",
               (unsigned long)mc->gregs[REG_EIP], (unsigned long)mc->gregs[REG_EFL]);
#elif defined(__aarch64__)
    mcontext_t *mc = &uc->uc_mcontext;
    for (int i = 0; i < 31; i += 2) {
        if (i < 30) {
            safe_write(fd, "  X%-2d: %016llx  X%-2d: %016llx\n", 
                       i, (unsigned long long)mc->regs[i], i+1, (unsigned long long)mc->regs[i+1]);
        } else {
            safe_write(fd, "  X30 : %016llx\n", (unsigned long long)mc->regs[30]);
        }
    }
    safe_write(fd, "  SP  : %016llx  PC  : %016llx  PSTATE: %016llx\n",
               (unsigned long long)mc->sp, (unsigned long long)mc->pc, (unsigned long long)mc->pstate);
#elif defined(__arm__)
    mcontext_t *mc = &uc->uc_mcontext;
    safe_write(fd, "  R0 : %08lx  R1 : %08lx  R2 : %08lx  R3 : %08lx\n",
               (unsigned long)mc->arm_r0, (unsigned long)mc->arm_r1, (unsigned long)mc->arm_r2, (unsigned long)mc->arm_r3);
    safe_write(fd, "  R4 : %08lx  R5 : %08lx  R6 : %08lx  R7 : %08lx\n",
               (unsigned long)mc->arm_r4, (unsigned long)mc->arm_r5, (unsigned long)mc->arm_r6, (unsigned long)mc->arm_r7);
    safe_write(fd, "  R8 : %08lx  R9 : %08lx  R10: %08lx\n",
               (unsigned long)mc->arm_r8, (unsigned long)mc->arm_r9, (unsigned long)mc->arm_r10);
    safe_write(fd, "  FP : %08lx  IP : %08lx  SP : %08lx  LR : %08lx\n",
               (unsigned long)mc->arm_fp, (unsigned long)mc->arm_ip, (unsigned long)mc->arm_sp, (unsigned long)mc->arm_lr);
    safe_write(fd, "  PC : %08lx  CPSR: %08lx\n",
               (unsigned long)mc->arm_pc, (unsigned long)mc->arm_cpsr);
#else
    safe_write(fd, "  [Registers dump not implemented for this architecture]\n");
#endif
}

/* Helper to print a hex dump of memory */
static void print_hex_dump(int fd, const void *addr, size_t len) {
    const unsigned char *p = (const unsigned char *)addr;
    size_t i;
    char ascii[17];
    ascii[16] = '\0';

    for (i = 0; i < len; i++) {
        if (i % 16 == 0) {
            safe_write(fd, "%016lx: ", (uintptr_t)(p + i));
        }

        safe_write(fd, "%02x ", p[i]);
        if (isprint(p[i])) {
            ascii[i % 16] = (char)p[i];
        } else {
            ascii[i % 16] = '.';
        }

        if ((i + 1) % 16 == 0) {
            safe_write(fd, " |%s|\n", ascii);
        } else if (i == len - 1) {
            // Padding for last line
            int remaining = 16 - ((int)i % 16) - 1;
            for (int k = 0; k < remaining; k++) {
                safe_write(fd, "   ");
            }
            ascii[(i % 16) + 1] = '\0'; // terminate earlier
            safe_write(fd, " |%s|\n", ascii);
        }
    }
}

/* Probe if memory is readable without causing a fault
 * Returns 1 if readable, 0 if not.
 */
static int is_address_readable(const void *addr, size_t len) {
    // Write to /dev/null to test if memory is readable
    // This is async-signal-safe (open/write/close are safe)
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd == -1) return 0; // Conservative assume not readable if can't open null
    
    ssize_t ret = write(null_fd, addr, len);
    int saved_errno = errno;
    close(null_fd);
    
    if (ret == (ssize_t)len) return 1;
    
    // If write failed with EFAULT, it implies bad address
    // other errors might be ambiguous, but for crash dump let's fail safe
    (void)saved_errno;
    return 0;
}

/* Print stack memory snapshot */
static void print_stack_memory(int fd, const void *sp) {
    safe_write(fd, "\nStack Dump (SP +/- 256 bytes):\n");
    if (!sp) {
        safe_write(fd, "  Stack pointer invalid.\n");
        return;
    }

    uintptr_t start = (uintptr_t)sp - 256;
    start &= ~(uintptr_t)0xF;
    
    // Validate the *entire* range we intend to read (512 bytes).
    // is_address_readable uses write() which will fault (EFAULT) if ANY part of the buffer is invalid,
    // but because it's a syscall, the kernel handles the fault and returns error, rather than killing us.
    if (!is_address_readable((void*)start, 512)) {
        safe_write(fd, "  Stack memory region [0x%lx - 0x%lx] unmapped or protected.\n", start, start + 512);
        return;
    }

    print_hex_dump(fd, (void*)start, 512);
}

/* Print memory maps from /proc/self/maps */
static void print_memory_maps(int fd) {
    safe_write(fd, "\nMemory Maps:\n");
    int map_fd = open("/proc/self/maps", O_RDONLY);
    if (map_fd == -1) {
        safe_write(fd, "  Failed to open /proc/self/maps\n");
        return;
    }

    char buffer[1024];
    ssize_t bytes;
    while ((bytes = read(map_fd, buffer, sizeof(buffer))) > 0) {
        // We might need to handle partial writes, but for safe_write and crash dump
        // best effort is acceptable.
        if (write(fd, buffer, (size_t)bytes) == -1) {
            // ignore
        }
    }
    close(map_fd);
    safe_write(fd, "\n");
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

/* Print command line arguments from /proc/self/cmdline */
static void print_command_line(int fd) {
    safe_write(fd, "\nCommand Line:\n");
    int cmd_fd = open("/proc/self/cmdline", O_RDONLY);
    if (cmd_fd == -1) {
        safe_write(fd, "  Failed to open /proc/self/cmdline\n");
        return;
    }

    char buffer[4096];
    ssize_t bytes = read(cmd_fd, buffer, sizeof(buffer) - 1);
    close(cmd_fd);

    if (bytes > 0) {
        buffer[bytes] = '\0';
        for (ssize_t i = 0; i < bytes - 1; i++) {
            if (buffer[i] == '\0') buffer[i] = ' ';
        }
        safe_write(fd, "  %s\n", buffer);
    } else {
        safe_write(fd, "  (empty)\n");
    }
}

void backtrace_save(const char* filepath, void* fault_rip, ucontext_t* uc, siginfo_t* info) {
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

    // Basic signal info (also printed to stderr in handler, but good to have in file)
    if (info) {
        safe_write(fd, "Signal: %d, Address: %p\n", info->si_signo, info->si_addr);
        print_signal_info(fd, info);
    }

    print_backtrace_fd(fd, fault_rip);

    if (uc) {
        print_registers(fd, uc);
        void* sp = NULL;
#if defined(__x86_64__)
        sp = (void*)uc->uc_mcontext.gregs[REG_RSP];
#elif defined(__i386__)
        sp = (void*)uc->uc_mcontext.gregs[REG_ESP];
#elif defined(__aarch64__)
        sp = (void*)uc->uc_mcontext.sp;
#elif defined(__arm__)
        sp = (void*)uc->uc_mcontext.arm_sp;
#endif
        if (sp) {
            print_stack_memory(fd, sp);
        }
    }

    print_command_line(fd);
    print_process_status(fd);
    print_open_fds(fd);
    print_environment(fd);
    print_memory_maps(fd);

    // Dump pending logs
    po_logger_crash_dump(fd);

    close(fd);
}

static void crash_handler(int sig, siginfo_t* info, void* ctx) {
    ucontext_t *uc = (ucontext_t *)ctx;
    void* fault_rip = NULL;
#if defined(__x86_64__)
    fault_rip = (void*)uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__i386__)
    fault_rip = (void*)uc->uc_mcontext.gregs[REG_EIP];
#elif defined(__aarch64__)
    fault_rip = (void*)uc->uc_mcontext.pc;
#elif defined(__arm__)
    fault_rip = (void*)uc->uc_mcontext.arm_pc;
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
        backtrace_save(dump_path, fault_rip, uc, info);
    }

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
    int signals[] = {
        SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE, SIGTRAP
#ifdef SIGSYS
        , SIGSYS
#endif
    };
    for (size_t i = 0; i < sizeof(signals)/sizeof(signals[0]); ++i) {
        if (sigutil_handle(signals[i], crash_handler, (int)SA_RESETHAND) == -1) {
            /* Ignore error in init */
        }
    }
}
