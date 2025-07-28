// include/postoffice/utils/logging.h
#ifndef PO_UTILS_LOGGING_H
#define PO_UTILS_LOGGING_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include "log_c/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the asynchronous logging subsystem.
 *
 * @param threads    Number of producer threads that will emit logs.
 * @param queue_size Capacity of each thread-local log queue.
 * @param level      Global minimum log level (LOG_TRACE..LOG_FATAL).
 * @return int       0 on success, non-zero on error.
 */
int logging_init(size_t threads, size_t queue_size, int level);

/**
 * @brief Shutdown the logging subsystem, flush all pending logs.
 */
void logging_shutdown(void);

/**
 * @brief Add a FILE* sink for log messages.
 *
 * Messages at or above level will be written to fp.
 *
 * @param fp    File pointer (e.g. stdout or a log file).
 * @param level Minimum log level to emit to this sink.
 * @return int  0 on success, non-zero on failure.
 */
int logging_add_file(FILE *fp, int level);

/**
 * @brief Add a custom callback sink for log messages.
 *
 * @param fn    Callback function invoked with a log_Event.
 * @param udata User data passed to the callback.
 * @param level Minimum log level to invoke this callback.
 * @return int  0 on success, non-zero on failure.
 */
int logging_add_callback(log_LogFn fn, void *udata, int level);

/**
 * @brief Set the global minimum log level at runtime.
 *
 * @param level New global log level (LOG_TRACE..LOG_FATAL).
 */
int logging_set_level(int level);

/**
 * @brief Check if a log level is currently enabled by the global setting.
 *
 * Can be used to guard expensive log calls.
 *
 * @param level  Log level to check.
 * @return true  If level >= current global level.
 * @return false Otherwise.
 */
bool logging_level_enabled(int level);

/**
 * @brief Enqueue a log message for asynchronous processing.
 *
 * Do not call directly; use the PO_LOG or log_* macros below.
 *
 * @param level Log level.
 * @param file  Source file name (__FILE__).
 * @param line  Source line number (__LINE__).
 * @param fmt   printf-style format.
 * @param ...   Format arguments.
 */
void logging_enqueue(
    int level,
    const char *file,
    int line,
    const char *fmt,
    ...
) __attribute__((format(printf, 4, 5)));

/**
 * @def PO_LOG
 * @brief Core logging macro: cheap check then enqueue.
 *
 * @param level Log level (use LOG_TRACE..LOG_FATAL).
 * @param ...   printf-style args.
 */
#define PO_LOG(level, ...)                           \
    do {                                             \
        if (logging_level_enabled(level)) {          \
            logging_enqueue(level, __FILE__, __LINE__, __VA_ARGS__); \
        }                                            \
    } while (0)

// Convenience macros matching log_c levels
#undef log_trace
#define log_trace(...) PO_LOG(LOG_TRACE, __VA_ARGS__)
#undef log_debug
#define log_debug(...) PO_LOG(LOG_DEBUG, __VA_ARGS__)
#undef log_info
#define log_info(...)  PO_LOG(LOG_INFO,  __VA_ARGS__)
#undef log_warn
#define log_warn(...)  PO_LOG(LOG_WARN,  __VA_ARGS__)
#undef log_error
#define log_error(...) PO_LOG(LOG_ERROR, __VA_ARGS__)
#undef log_fatal
#define log_fatal(...) PO_LOG(LOG_FATAL, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // PO_UTILS_LOGGING_H
