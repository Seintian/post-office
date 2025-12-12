#ifndef BACKTRACE_H
#define BACKTRACE_H

#include <ucontext.h>
#include <signal.h>

/**
 * @brief Initialize the backtrace library.
 *
 * This function installs the signal handlers to catch crashes (SIGSEGV, etc.)
 * and print/save backtraces.
 *
 * @param crash_dump_dir Directory to save crash reports. If NULL, reports are not saved to files, only printed to stderr.
 */
void backtrace_init(const char* crash_dump_dir);

/**
 * @brief Print the current stacktrace to stderr.
 *
 * Captures the current stack and prints it immediately.
 */
void backtrace_print(void);

/**
 * @brief Save the current stacktrace and memory snapshot to a specific file.
 *
 * @param filepath Path to the file where the stacktrace will be saved.
 * @param fault_rip Faulting instruction pointer (optional, can be NULL).
 * @param uc User context from signal handler containing registers (optional, can be NULL).
 * @param info Signal info containing crash details (optional, can be NULL).
 */
void backtrace_save(const char* filepath, void* fault_rip, ucontext_t* uc, siginfo_t* info);

#endif // BACKTRACE_H
