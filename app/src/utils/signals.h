/**
 * @file signals.h
 * @brief Utility functions for signal handling in C.
 *
 * This header file provides utility functions and macros for managing
 * signal handling in C programs. It includes functions for initializing
 * signal handling, blocking/unblocking signals, and setting custom
 * signal handlers.
 * 
 * @see signals.c
 */

#ifndef _UTILS_SIGNALS_H
#define _UTILS_SIGNALS_H

#ifndef _GNU_SOURCE
// Required by IDE
#define _GNU_SOURCE
#endif

#include <signal.h>
#include <stdbool.h>


/**
 * @def COMMON_TERMINATING_SIGNALS
 * @brief List of signals that cause the application to terminate.
 *
 * This macro defines a list of common signals that cause the application to terminate.
 * 
 * @note This list includes the following signals:
 * - SIGINT
 * - SIGTERM
 * - SIGABRT
 * - SIGHUP
 * - SIGQUIT
 * - SIGABRT
 * - SIGFPE
 * - SIGSEGV
 * - SIGPIPE
 * - SIGALRM
 * - SIGILL
 * - SIGBUS
 * - SIGSYS
 */
#define COMMON_TERMINATING_SIGNALS \
    SIGINT,  \
    SIGTERM, \
    SIGABRT, \
    SIGHUP,  \
    SIGQUIT, \
    SIGABRT, \
    SIGFPE,  \
    SIGSEGV, \
    SIGPIPE, \
    SIGALRM, \
    SIGILL,  \
    SIGBUS,  \
    SIGSYS,  \

// Bitwise flags for selecting signal handling options
#define SIGUTIL_BLOCK_NONE              0x00  /** Do not block any signals */
#define SIGUTIL_BLOCK_TERMINATING_ONLY  0x01  /** Block only terminating signals */
#define SIGUTIL_BLOCK_NON_TERMINATING   0x02  /** Block only non-terminating signals */
// ...
#define SIGUTIL_BLOCK_ALL_SIGNALS       0x0f  /** Block all signals */

#define SIGUTIL_HANDLE_NONE             0x00  /** Do not handle any signals */
#define SIGUTIL_HANDLE_TERMINATING_ONLY 0x10  /** Handle only terminating signals */
#define SIGUTIL_HANDLE_NON_TERMINATING  0x20  /** Handle only non-terminating signals */
// ...
#define SIGUTIL_HANDLE_ALL_SIGNALS      0xf0  /** Handle all signals */

/**
 * @typedef signals_handler_t
 * @brief Custom signal handler function type.
 *
 * This type defines a custom signal handler function that can be used
 * to handle signals in a C program.
 *
 * @param[in] sig The signal number.
 * @param[in] info Signal information.
 * @param[in] context Signal context.
 * 
 * @note Handler set with `SIGINFO` flag.
 * @note `context` can be casted to `ucontext_t` for further processing.
 */
typedef void (*signals_handler_t)(int sig, siginfo_t* info, void* context);

/**
 * @brief Initialize signal handling with a given handler and options.
 *
 * This function sets up signal handling for a specified set of signals using
 * the provided handler and options.
 *
 * @param[in] handler The custom signal handler function.
 * It can be `NULL` if flag is in range `[0x00, 0x0f]`.
 * Otherwise, it must be a valid function pointer.
 *
 * @param[in] flag The signal handling options:
 * - `SIGUTIL_BLOCK_NONE`: Do not block any signals: `0x00`
 * - `SIGUTIL_BLOCK_TERMINATING_ONLY`: Block only terminating signals: `0x01`
 * - `SIGUTIL_BLOCK_NON_TERMINATING`: Block only non-terminating signals: `0x02`
 * - `SIGUTIL_BLOCK_ALL_SIGNALS`: Block all signals: `0x0f`
 * - `SIGUTIL_HANDLE_NONE`: Do not handle any signals: `0x00`
 * - `SIGUTIL_HANDLE_TERMINATING_ONLY`: Handle only terminating signals: `0x10`
 * - `SIGUTIL_HANDLE_NON_TERMINATING`: Handle only non-terminating signals: `0x20`
 * - `SIGUTIL_HANDLE_ALL_SIGNALS`: Handle all signals: `0xf0`
 * 
 * @param[in] handler_flags The signal handler mask's flags.
 * @return 0 on success, -1 on failure.
 */
int sigutil_setup(signals_handler_t handler, unsigned char flag, int handler_flags) __wur;

/**
 * @brief Handle a single signal with a custom signal handler.
 *
 * @param[in] signum The signal number.
 * @param[in] handler The custom signal handler function.
 * @param[in] flags The signal handler mask's flags
 * @return 0 on success, -1 on failure.
 */
int sigutil_handle(int signum, signals_handler_t handler, int flags) __nonnull((2)) __wur;

/**
 * @brief Handle all signals with a custom signal handler.
 *
 * @param[in] handler The custom signal handler function.
 * @param[in] flags The signal handler mask's flags
 * @return 0 on success, -1 on failure.
 */
int sigutil_handle_all(signals_handler_t handler, int flags) __nonnull((1)) __wur;

/**
 * @brief Handle only terminating signals with a custom signal handler.
 *
 * @param[in] handler The custom signal handler function.
 * @param[in] flags The signal handler mask's flags
 * @return 0 on success, -1 on failure.
 */
int sigutil_handle_terminating(signals_handler_t handler, int flags) __nonnull((1)) __wur;

/**
 * @brief Handle only non-terminating signals with a custom signal handler.
 *
 * @param[in] handler The custom signal handler function.
 * @param[in] flags The signal handler mask's flags
 * @return 0 on success, -1 on failure.
 */
int sigutil_handle_non_terminating(signals_handler_t handler, int flags) __nonnull((1)) __wur;

/**
 * @brief Block a single signal.
 *
 * @param[in] signum The signal number.
 * @return 0 on success, -1 on failure.
 */
int sigutil_block(int signum) __wur;

/**
 * @brief Block all signals.
 *
 * @return 0 on success, -1 on failure.
 */
int sigutil_block_all(void) __wur;

/**
 * @brief Block only terminating signals.
 *
 * @return 0 on success, -1 on failure.
 */
int sigutil_block_terminating(void) __wur;

/**
 * @brief Block only non-terminating signals.
 *
 * @return 0 on success, -1 on failure.
 */
int sigutil_block_non_terminating(void) __wur;

/**
 * @brief Unblock a single signal.
 *
 * @param[in] signum The signal number.
 * @return 0 on success, -1 on failure.
 */
int sigutil_unblock(int signum) __wur;

/**
 * @brief Unblock all signals.
 *
 * @return 0 on success, -1 on failure.
 */
int sigutil_unblock_all(void) __wur;

/**
 * @brief Retrieves the set of currently blocked signals.
 *
 * @param[out] set A pointer to a sigset_t structure where the blocked signals will be stored.
 * @return 0 on success, or -1 on failure.
 */
int sigutil_get_blocked_signals(sigset_t* set) __nonnull((1)) __wur;

/**
 * @brief Restore the default signal handler for a specific signal.
 *
 * @param[in] signum The signal number.
 * @return 0 on success, -1 on failure.
 */
int sigutil_restore(int signum) __wur;

/**
 * @brief Restore all signals to default.
 * 
 * @return 0 on success, -1 on failure.
 */
int sigutil_restore_all(void) __wur;

/**
 * @brief Wait for a signal to be received.
 *
 * This function blocks until the specified signal is received.
 *
 * @param[in] signum The signal number.
 * @return 0 on success, -1 on failure.
 * 
 * @note This function blocks the signal to wait it and unblocks it when caught.
 */
int sigutil_wait(int signum) __wur;

/**
 * @brief Wait for any signal to be received.
 *
 * This function blocks until any signal is received.
 *
 * @return signal caught's number on success, -1 on failure.
 * 
 * @note This function blocks all *blockable* signals to wait any signal
 * and unblocks them when caught.
 */
int sigutil_wait_any(void) __wur;

/**
 * @brief Signal children with a specified signal.
 * 
 * This function sends a specified signal to all child processes of the current
 * process. It temporarily changes the process group ID to ensure that the signal
 * is sent to the correct group.
 * 
 * @param[in] sig The signal to send to the children.
 * @return 0 on success, -1 on failure.
 * 
 * @note This function is used to send a signal to all children of the current
 *       process.
 */
int sigutil_signal_children(int sig);

#endif // _UTILS_SIGNALS_H