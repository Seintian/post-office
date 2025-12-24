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
#define LOGGER_SINK_CONSOLE (1u << 0) /**< Log to console (deprecated alias for STDERR) */
#define LOGGER_SINK_FILE (1u << 1)    /**< Log to file */
#define LOGGER_SINK_SYSLOG (1u << 2)  /**< Log to syslog */
#define LOGGER_SINK_STDOUT (1u << 3)  /**< Log to stdout */
#define LOGGER_SINK_STDERR (1u << 4)  /**< Log to stderr */

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
 * This function initializes the shared logger state, including the ring buffer,
 * worker threads, and internal structures. It must be called before any other
 * logger function.
 *
 * @param[in] cfg Configuration parameters (must not be NULL). The logger copies
 *                the necessary values, so the caller retains ownership of the struct.
 * @return 0 on success, -1 on error (errno set).
 *
 * @note Thread-safe: No (Must be called from the main thread before spawning others).
 * @warning Calling this function multiple times without shutdown is undefined behavior.
 */
int po_logger_init(const po_logger_config_t *cfg) __nonnull((1));

/**
 * @brief Shutdown the logger and release all resources.
 *
 * Blocks until all worker threads terminate and any remaining queued messages
 * are flushed to configured sinks. Safe to call once after a successful
 * @ref po_logger_init.
 *
 * @note Thread-safe: No (Must be called from the main thread after joining others).
 */
void po_logger_shutdown(void);

/**
 * @brief Set the runtime logging level.
 *
 * Messages with a severity lower than the specified level will be dropped
 * immediately by the producer to minimize overhead.
 *
 * @param[in] level Minimum level to log (messages below this level are dropped).
 * @return 0 on success, -1 if the level is invalid.
 *
 * @note Thread-safe: Yes (Atomic update).
 */
int po_logger_set_level(po_log_level_t level);

/**
 * @brief Get the current runtime logging level.
 *
 * @return The current logging level.
 *
 * @note Thread-safe: Yes (Atomic read).
 */
po_log_level_t po_logger_get_level(void);

/**
 * @brief Parse log level from string.
 *
 * Converts a string representation (e.g., "INFO") to the corresponding enum value.
 * Case-insensitive.
 *
 * @param[in] str String representation (TRACE, DEBUG, INFO, WARN, ERROR, FATAL).
 *                (Nullable: if NULL, returns -1).
 * @return Log level or -1 if invalid or NULL.
 *
 * @note Thread-safe: Yes (Pure function).
 */
int po_logger_level_from_str(const char *str);

/**
 * @brief Add console output as a log sink.
 *
 * Configures the logger to write messages to stdout or stderr.
 *
 * @param[in] use_stderr If true, use stderr; otherwise use stdout.
 * @return 0 on success, -1 on error.
 *
 * @note Thread-safe: No (Ideally call during initialization).
 */
int po_logger_add_sink_console(bool use_stderr);

/**
 * @brief Add file output as a log sink.
 *
 * Opens the specified file for writing/appending. The logger takes ownership
 * of the file handle and closes it on shutdown.
 *
 * @param[in] path Path to the log file (must not be NULL).
 * @param[in] append If true, append to the file; otherwise overwrite.
 * @return 0 on success, -1 on error.
 *
 * @note Thread-safe: No (Ideally call during initialization).
 */
int po_logger_add_sink_file(const char *path, bool append);

/**
 * @brief Add file output as a categorized log sink.
 * 
 * Similar to po_logger_add_sink_file but only records messages matching the category mask.
 * 
 * @param[in] path Path to the log file.
 * @param[in] append If true, append to the file.
 * @param[in] category_mask Bitmask of categories to accept (e.g., (1u << category)).
 *                          If 0, accepts ALL categories (behaves like po_logger_add_sink_file).
 * @return 0 on success, -1 on error.
 */
int po_logger_add_sink_file_categorized(const char *path, bool append, uint32_t category_mask);

/**
 * @brief Add syslog as a log sink.
 *
 * @param[in] ident Identity string for syslog messages (Nullable).
 *                  If not NULL, the logger duplicates the string (internal ownership).
 * @return 0 on success, -1 on error.
 *
 * @note Thread-safe: No (Ideally call during initialization).
 */
int po_logger_add_sink_syslog(const char *ident);

/**
 * @brief Add a custom log sink.
 *
 * Registers a callback function to be called for each formatted log message.
 *
 * @param[in] fn Callback function to handle log messages (must not be NULL).
 * @param[in] udata User data to pass to the callback (Ownership passed to callback).
 * @return 0 on success, -1 on error.
 *
 * @note Thread-safe: No (Ideally call during initialization).
 */
int po_logger_add_sink_custom(void (*fn)(const char *line, void *udata), void *udata);

/**
 * @brief Set the log category for the current thread.
 * 
 * All subsequent log messages from this thread will be tagged with this category.
 * 
 * @param[in] category Category ID (0-31 recommended for mask support).
 */
void po_logger_set_thread_category(uint32_t category);

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
 * @param[in] level Log level.
 * @param[in] file Source file name (Usually __FILE__).
 * @param[in] line Source line number (Usually __LINE__).
 * @param[in] func Function name (Usually __func__).
 * @param[in] fmt Format string (printf-style, must not be NULL).
 * @param[in] ... Arguments for the format string.
 *
 * @note Thread-safe: Yes (High-throughput, async, lock-free producer).
 */
void po_logger_log(po_log_level_t level, const char *file, int line, const char *func,
                   const char *fmt, ...) __attribute__((format(printf, 5, 6)));

/**
 * @brief Log a message with the specified level, location, and va_list.
 *
 * This is the varargs version of po_logger_log, used by the implementation.
 *
 * @param[in] level Log level.
 * @param[in] file Source file name.
 * @param[in] line Source line number.
 * @param[in] func Function name.
 * @param[in] fmt Format string (printf-style).
 * @param[in] ap Variable argument list.
 *
 * @note Thread-safe: Yes.
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
