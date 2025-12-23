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
 * @param[in] crash_dump_dir Directory to save crash reports. If NULL, reports are not saved to files, only printed to stderr.
 * @note Thread-safe: No (Modifies global signal handlers).
 */
void backtrace_init(const char* crash_dump_dir);

/**
 * @brief Print the current stacktrace to stderr.
 *
 * Captures the current stack and prints it immediately.
 * @note Thread-safe: Yes.
 */
void backtrace_print(void);

/**
 * @brief Save the current stacktrace and memory snapshot to a specific file.
 *
 * @param[in] filepath Path to the file where the stacktrace will be saved.
 * @param[in] fault_rip Faulting instruction pointer (optional, can be NULL).
 * @param[in] uc User context from signal handler containing registers (optional, can be NULL).
 * @param[in] info Signal info containing crash details (optional, can be NULL).
 * @note Async-signal-safe: No (Uses fork/exec for symbol resolution).
 */
void backtrace_save(const char* filepath, void* fault_rip, ucontext_t* uc, siginfo_t* info);

#endif // BACKTRACE_H
