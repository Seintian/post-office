#ifndef PO_LOG_LOGGER_H
#define PO_LOG_LOGGER_H

/**
 * @file logger.h
 * @brief High-throughput, thread-safe asynchronous logger for PostOffice.
 *
 * @defgroup logger Logging subsystem
 * @brief Asynchronous, lock-free logging API and configuration.
 *
 * This module provides a non-blocking logging API using a lock-free MPMC
 * ring buffer and a background consumer thread to drain records to the
 * configured sinks (console, file, syslog).
 *
 * @{
 * - Lock-free ring buffer for pending records (work queue)
 * - Preallocated record pool with a lock-free freelist to avoid malloc/free in hot paths
 * Design goals:
 * - Non-blocking hot path for producers via a lock-free MPMC ring buffer
 *   (see @ref perf_ringbuf_t).
 * - Dedicated consumer thread(s) drain the ring and write to configured sinks
 *   (console, file, syslog). Number of consumers is configurable at init.
 * - No dynamic allocations in the hot path; log records are fixed-size and
 *   messages longer than the capacity are truncated.
 * - Cheap fast-path macros (LOG_TRACE..LOG_FATAL) that avoid formatting when
 *   the current compile-time/runtime log level disables the call.
 *
 * Reliability vs performance trade-offs:
 * - When the ring is full, writers never block. By default the newest message
 *   overwrites the oldest not-yet-processed one to keep the system making
 *   forward progress ("drop oldest on overflow"). This avoids head-of-line
 *   blocking at the cost of potentially losing very old messages under bursty
 *   loads. See logger_overflow_policy.
 *
 * Usage example:
 * @code
 * #include <postoffice/log/logger.h>
 *
 * int main(void) {
 *     po_logger_config_t cfg = {
 *         .level = LOG_INFO,
 *         .ring_capacity = 1u << 14, // 16384 records
 *         .consumers = 1,
 *         .policy = LOGGER_OVERWRITE_OLDEST,
 *     };
 *     if (po_logger_init(&cfg) != 0) return 1;
 *     po_logger_add_sink_console(true);            // stderr, color
 *     po_logger_add_sink_file("/tmp/po.log", true); // append
 *
 *     LOG_INFO("service started pid=%d", (int)getpid());
 *     LOG_DEBUG("debug value=%d", 42); // printed only if level <= DEBUG
 *
 *     logger_shutdown();
 *     return 0;
 * }
 * @endcode
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum logger_level
 * @brief Logging severity levels (ascending order of severity)
 */
// --- Levels (ordered ascending) ---
typedef enum po_logger_level {
    LOG_TRACE = 0,
    LOG_DEBUG = 1,
    LOG_INFO = 2,
    LOG_WARN = 3,
    LOG_ERROR = 4,
    LOG_FATAL = 5
} po_log_level_t;

/**
 * @enum logger_overflow_policy
 * @brief Behavior applied when the pending-record queue is full.
 */
// Overflow policy when the ring is full.
typedef enum po_logger_overflow_policy {
    LOGGER_DROP_NEW = 0,        // drop the incoming message (cheap)
    LOGGER_OVERWRITE_OLDEST = 1 // advance head (drop oldest), then write
} po_logger_overflow_policy_t;

// Sinks mask
#define LOGGER_SINK_CONSOLE (1u << 0)
#define LOGGER_SINK_FILE (1u << 1)
#define LOGGER_SINK_SYSLOG (1u << 2)

// Compile-time default level (can be overridden with -DLOGGER_COMPILE_LEVEL=N)
#ifndef LOGGER_COMPILE_LEVEL
#define LOGGER_COMPILE_LEVEL LOG_TRACE
#endif

// Log record constants
#define LOGGER_MSG_MAX 512u // bytes for formatted message (truncated)

/**
 * @struct logger_config
 * @brief Initialization parameters for the logger.
 *
 * The logger owns any resources it opens during @ref logger_init and releases
 * them during @ref logger_shutdown.
 */
// Public config
typedef struct po_logger_config {
    po_log_level_t level; /**< Minimum runtime level (messages below are discarded fast-path). */
    size_t ring_capacity; /**< Capacity of the internal ring buffer (prefer power-of-two). */
    unsigned consumers;   /**< Number of consumer threads draining the queue. */
    po_logger_overflow_policy_t policy; /**< Overflow behavior when queue is full. */
    size_t
        cacheline_bytes; /**< Optional hardware cacheline size hint for internal ring buffers.
                              If 0, a default of 64 is used. Must be a power of two if provided. */
} po_logger_config_t;

// Initialization and control
/**
 * @brief Initialize the asynchronous logger.
 *
 * @param cfg Configuration parameters (must not be NULL).
 * @return 0 on success, -1 on error (errno may be set).
 *
 * Notes:
 * - Spawns @ref logger_config.consumers background worker threads.
 * - Allocates a pre-sized record pool; no allocations occur on the hot path.
 */
int po_logger_init(const po_logger_config_t *cfg) __nonnull((1));

/**
 * @brief Shutdown the logger and release all resources.
 *
 * Blocks until all worker threads terminate and any remaining queued messages
 * are flushed to configured sinks. Safe to call once after a successful
 * @ref logger_init.
 */
void po_logger_shutdown(void);

/**
 * @brief Set the minimum runtime logging level.
 *
 * @param level New minimum level.
 * @return 0 on success; -1 if the logger is not initialized.
 */
int po_logger_set_level(po_log_level_t level);

/**
 * @brief Get the current minimum runtime logging level.
 * @return Current level; if uninitialized, returns the compilation default.
 */
po_log_level_t po_logger_get_level(void);

// Sinks
/**
 * @brief Enable console sink output.
 *
 * @param use_stderr When true, write to stderr; otherwise stdout.
 * @return 0 on success; -1 on error.
 */
int po_logger_add_sink_console(bool use_stderr);

/**
 * @brief Enable file sink output.
 *
 * Opens or creates the specified file path and appends or truncates based on
 * @p append.
 *
 * @param path   Filesystem path to the log file (must not be NULL).
 * @param append When true, append to existing file; otherwise truncate.
 * @return 0 on success; -1 on error (errno may be set).
 */
int po_logger_add_sink_file(const char *path, bool append) __nonnull((1));

/**
 * @brief Enable syslog sink output.
 *
 * @param ident Syslog ident string (may be NULL for default).
 * @return 0 on success; -1 if unavailable or on error.
 */
int po_logger_add_sink_syslog(const char *ident);

/**
 * @brief Add a custom sink that receives formatted log lines.
 *
 * The callback is invoked from logger worker threads with a NUL-terminated
 * formatted line (no trailing newline trimming guaranteed). The callback
 * must be non-blocking and fast.
 *
 * @param fn   Callback function pointer
 * @param udata User pointer passed to callback
 * @return 0 on success, -1 on error
 */
int po_logger_add_sink_custom(void (*fn)(const char *line, void *udata), void *udata)
    __nonnull((1));

// Fast-path check
/**
 * @brief Fast-path check to determine if a message at @p level would be logged.
 *
 * This macro-friendly inline avoids formatting costs when the message would be
 * filtered by compile-time or runtime level.
 *
 * @param level Candidate message level.
 * @return true if the message would be accepted by the current filters.
 */
static inline bool logger_would_log(po_log_level_t level) {
    extern volatile int _logger_runtime_level; // defined in logger.c
    return (level >= (po_log_level_t)LOGGER_COMPILE_LEVEL) &&
           (level >= (po_log_level_t)_logger_runtime_level);
}

// Core enqueue API (avoid using directly; prefer macros)
/**
 * @brief Core varargs-enqueue primitive.
 *
 * Prefer the LOG_* convenience macros which invoke this with call-site
 * metadata and avoid formatting costs when disabled.
 *
 * @param level Message severity level.
 * @param file  Source file path (typically __FILE__).
 * @param line  Source line number (typically __LINE__).
 * @param func  Function name (typically __func__).
 * @param fmt   printf-style format string.
 * @param ap    Varargs list.
 */
void po_logger_logv(po_log_level_t level, const char *file, int line, const char *func,
                    const char *fmt, va_list ap) __attribute__((format(printf, 5, 0)));

/**
 * @brief Core enqueue primitive.
 *
 * @param level Message severity level.
 * @param file  Source file path (typically __FILE__).
 * @param line  Source line number (typically __LINE__).
 * @param func  Function name (typically __func__).
 * @param fmt   printf-style format followed by arguments.
 */
void po_logger_log(po_log_level_t level, const char *file, int line, const char *func,
                   const char *fmt, ...) __attribute__((format(printf, 5, 6)));

// Convenience macros that avoid formatting when disabled
/**
 * @def LOG_ENABLED
 * @brief Check if a level would pass filters (compile-time and runtime).
 */
#define LOG_ENABLED(lvl) logger_would_log(lvl)

/**
 * @def LOG_AT
 * @brief Log at the given level with printf-style formatting.
 */
#define LOG_AT(lvl, fmt, ...)                                                                      \
    do {                                                                                           \
        if (LOG_ENABLED(lvl)) {                                                                    \
            po_logger_log((lvl), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__);              \
        }                                                                                          \
    } while (0)

#define LOG_TRACE(fmt, ...) LOG_AT(LOG_TRACE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_AT(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG_AT(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG_AT(LOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_AT(LOG_ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) LOG_AT(LOG_FATAL, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

/** @} */ // end of logger group

#endif // PO_LOG_LOGGER_H
