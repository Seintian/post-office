/**
 * @file logger.h
 * @brief High-throughput, thread-safe asynchronous logger for PostOffice.
 *
 * @addtogroup logger
 * @brief Asynchronous, lock-free logging API and configuration.
 *
 * This module provides a non-blocking logging API using a lock-free MPMC
 * ring buffer and a background consumer thread to drain records to the
 * configured sinks (console, file, syslog).
 *
 * Key features:
 * - Lock-free ring buffer for pending records (work queue)
 * - Preallocated record pool with a lock-free freelist to avoid malloc/free in hot paths
 * - Non-blocking hot path for producers via a lock-free MPMC ring buffer
 * - Dedicated consumer thread(s) for writing to sinks
 * - No dynamic allocations in the hot path
 * - Configurable log levels and overflow policies
 *
 * @{
 */

#ifndef PO_LOG_LOGGER_H
#define PO_LOG_LOGGER_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum po_logger_level
 * @brief Logging severity levels (ascending order of severity)
 */
typedef enum po_logger_level {
    LOG_TRACE = 0, /**< Detailed debug information */
    LOG_DEBUG = 1, /**< Debug-level messages */
    LOG_INFO = 2,  /**< Informational messages */
    LOG_WARN = 3,  /**< Warning conditions */
    LOG_ERROR = 4, /**< Error conditions */
    LOG_FATAL = 5  /**< Fatal conditions that prevent further operation */
} po_log_level_t;

/**
 * @enum po_logger_overflow_policy
 * @brief Behavior applied when the pending-record queue is full.
 */
typedef enum po_logger_overflow_policy {
    LOGGER_DROP_NEW = 0,        /**< Drop the incoming message (cheap) */
    LOGGER_OVERWRITE_OLDEST = 1 /**< Advance head (drop oldest), then write */
} po_logger_overflow_policy_t;

/** Sink types that can be configured */
#define LOGGER_SINK_CONSOLE (1u << 0) /**< Log to console (stderr by default) */
#define LOGGER_SINK_FILE (1u << 1)    /**< Log to file */
#define LOGGER_SINK_SYSLOG (1u << 2)  /**< Log to syslog */

/** Compile-time default level (can be overridden with -DLOGGER_COMPILE_LEVEL=N) */
#ifndef LOGGER_COMPILE_LEVEL
#define LOGGER_COMPILE_LEVEL LOG_TRACE
#endif

/** Maximum length of a formatted log message (truncated if longer) */
#define LOGGER_MSG_MAX 512u

/**
 * @struct po_logger_config
 * @brief Initialization parameters for the logger.
 *
 * The logger owns any resources it opens during @ref po_logger_init and releases
 * them during @ref po_logger_shutdown.
 */
typedef struct po_logger_config {
    po_log_level_t level; /**< Minimum runtime level (messages below are discarded) */
    size_t ring_capacity; /**< Capacity of the internal ring buffer (power-of-two recommended) */
    unsigned consumers;   /**< Number of consumer threads (0 = auto-detect) */
    po_logger_overflow_policy_t policy; /**< Behavior when queue is full */
    size_t cacheline_bytes;             /**< Cache line size (0 = use default) */
} po_logger_config_t;

/**
 * @brief Initialize the logger with the given configuration.
 *
 * @param cfg Configuration parameters (must not be NULL).
 * @return 0 on success, -1 on error (errno set).
 */
int po_logger_init(const po_logger_config_t *cfg) __nonnull((1));

/**
 * @brief Shutdown the logger and release all resources.
 *
 * Blocks until all worker threads terminate and any remaining queued messages
 * are flushed to configured sinks. Safe to call once after a successful
 * @ref po_logger_init.
 */
void po_logger_shutdown(void);

/**
 * @brief Set the runtime logging level.
 *
 * @param level Minimum level to log (messages below this level are dropped).
 * @return 0 on success, -1 if the level is invalid.
 */
int po_logger_set_level(po_log_level_t level);

/**
 * @brief Get the current runtime logging level.
 *
 * @return The current logging level.
 */
po_log_level_t po_logger_get_level(void);

/**
 * @brief Parse log level from string.
 * @param str String representation (TRACE, DEBUG, INFO, WARN, ERROR, FATAL).
 * @return Log level or -1 if invalid.
 */
int po_logger_level_from_str(const char *str);

/**
 * @brief Add console output as a log sink.
 *
 * @param use_stderr If true, use stderr; otherwise use stdout.
 * @return 0 on success, -1 on error.
 */
int po_logger_add_sink_console(bool use_stderr);

/**
 * @brief Add file output as a log sink.
 *
 * @param path Path to the log file.
 * @param append If true, append to the file; otherwise overwrite.
 * @return 0 on success, -1 on error.
 */
int po_logger_add_sink_file(const char *path, bool append);

/**
 * @brief Add syslog as a log sink.
 *
 * @param ident Identity string for syslog messages.
 * @return 0 on success, -1 on error.
 */
int po_logger_add_sink_syslog(const char *ident);

/**
 * @brief Add a custom log sink.
 *
 * @param fn Callback function to handle log messages.
 * @param udata User data to pass to the callback.
 * @return 0 on success, -1 on error.
 */
int po_logger_add_sink_custom(void (*fn)(const char *line, void *udata), void *udata);

// Fast-path check to determine if a message at the given level would be logged.
static inline bool logger_would_log(po_log_level_t level) {
    extern volatile po_log_level_t _logger_runtime_level; // defined in logger.c
    return (level >= (po_log_level_t)LOGGER_COMPILE_LEVEL) && (level >= _logger_runtime_level);
}

// Core logging macro used by the convenience macros below.
#define LOG_AT(lvl, fmt, ...)                                                                      \
    do {                                                                                           \
        if (logger_would_log(lvl)) {                                                               \
            po_logger_log((lvl), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__);              \
        }                                                                                          \
    } while (0)

/** @def LOG_TRACE(fmt, ...)
 *  @brief Log a message at TRACE level.
 *  @param fmt Format string (printf-style).
 *  @param ... Arguments for the format string.
 */
#define LOG_TRACE(fmt, ...) LOG_AT(LOG_TRACE, fmt, ##__VA_ARGS__)

/** @def LOG_DEBUG(fmt, ...)
 *  @brief Log a message at DEBUG level.
 *  @param fmt Format string (printf-style).
 *  @param ... Arguments for the format string.
 */
#define LOG_DEBUG(fmt, ...) LOG_AT(LOG_DEBUG, fmt, ##__VA_ARGS__)

/** @def LOG_INFO(fmt, ...)
 *  @brief Log a message at INFO level.
 *  @param fmt Format string (printf-style).
 *  @param ... Arguments for the format string.
 */
#define LOG_INFO(fmt, ...) LOG_AT(LOG_INFO, fmt, ##__VA_ARGS__)

/** @def LOG_WARN(fmt, ...)
 *  @brief Log a message at WARNING level.
 *  @param fmt Format string (printf-style).
 *  @param ... Arguments for the format string.
 */
#define LOG_WARN(fmt, ...) LOG_AT(LOG_WARN, fmt, ##__VA_ARGS__)

/** @def LOG_ERROR(fmt, ...)
 *  @brief Log a message at ERROR level.
 *  @param fmt Format string (printf-style).
 *  @param ... Arguments for the format string.
 */
#define LOG_ERROR(fmt, ...) LOG_AT(LOG_ERROR, fmt, ##__VA_ARGS__)

/** @def LOG_FATAL(fmt, ...)
 *  @brief Log a message at FATAL level.
 *  @param fmt Format string (printf-style).
 *  @param ... Arguments for the format string.
 */
#define LOG_FATAL(fmt, ...) LOG_AT(LOG_FATAL, fmt, ##__VA_ARGS__)

/**
 * @brief Log a message with the specified level and location information.
 *
 * This is the low-level logging function used by the LOG_* macros.
 * Prefer using the macros instead of calling this directly.
 *
 * @param level Log level.
 * @param file Source file name.
 * @param line Source line number.
 * @param func Function name.
 * @param fmt Format string (printf-style).
 * @param ... Arguments for the format string.
 */
void po_logger_log(po_log_level_t level, const char *file, int line, const char *func,
                   const char *fmt, ...) __attribute__((format(printf, 5, 6)));

/**
 * @brief Log a message with the specified level, location, and va_list.
 *
 * This is the varargs version of po_logger_log, used by the implementation.
 *
 * @param level Log level.
 * @param file Source file name.
 * @param line Source line number.
 * @param func Function name.
 * @param fmt Format string (printf-style).
 * @param ap Variable argument list.
 */
void po_logger_logv(po_log_level_t level, const char *file, int line, const char *func,
                    const char *fmt, va_list ap);

/**
 * @brief Dump pending log messages from the ring buffer to a file descriptor.
 *
 * This function bypasses the writer thread and best-effort dumps all records
 * currently in the ring buffer. Intended for use in crash handlers.
 *
 * @note This function is async-signal-safe. It avoids dynamic allocation,
 *       locking, and stdio/formatting functions that are not signal-safe.
 *       It uses raw `write`, `strlen`, and manual ASCII formatting.
 *
 * @warning The provided file descriptor must be valid and writable.
 *          Since it accesses the shared ring buffer concurrently with potential
 *          producers (though arguably crashing), data consistency is best-effort.
 *          It is guaranteed not to deadlock or crash the handler itself.
 *
 * @param fd File descriptor to write to.
 */
void po_logger_crash_dump(int fd);

#ifdef __cplusplus
}
#endif

#endif /* PO_LOG_LOGGER_H */

/** @} */ // end of logger group
