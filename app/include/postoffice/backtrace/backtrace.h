#ifndef BACKTRACE_H
#define BACKTRACE_H

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
 * @brief Save the current stacktrace to a specific file.
 *
 * @param filepath Path to the file where the stacktrace will be saved.
 */
void backtrace_save(const char* filepath, void* fault_rip);

#endif // BACKTRACE_H
