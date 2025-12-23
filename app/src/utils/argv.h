/**
 * @file argv.h
 * @ingroup utils
 * @brief Command-line argument parsing helpers for post-office processes.
 *
 * Provides a light-weight, centralized parsing routine that normalizes CLI
 * flags into a `po_args_t` structure. Parsing is intentionally minimal and
 * avoids dynamic resizing beyond a few optional strings.
 */
#ifndef PO_UTILS_ARGV_H
#define PO_UTILS_ARGV_H
/** \ingroup utils */

#include <stdbool.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct po_args_t
 * @brief Parsed command-line arguments.
 *
 * Fields are initialized to defaults via `po_args_init()` and then populated
 * by `po_args_parse()`. Memory ownership: any allocated strings (e.g.,
 * `config_file`, `syslog_ident`) are freed by `po_args_destroy()`.
 */
typedef struct {
    bool help;          /**< `true` if --help was requested (implies early exit). */
    bool version;       /**< `true` if --version was requested (implies early exit). */
    char *config_file;  /**< Path to configuration file (may be NULL). */
    int loglevel;       /**< Logging verbosity level (application-defined scale). */

    /* Logging/syslog options */
    bool syslog;        /**< Enable syslog sink if true. */
    char *syslog_ident; /**< Optional ident for syslog (NULL => program name). */
    int fd;             /**< File descriptor for additional output (>=0) or -1 if unused. */

    /* Demo / mode flags */
    bool tui_demo;      /**< Run minimal TUI smoke demo then exit if true. */
    bool tui_sim;       /**< Run TUI with simulation */
} po_args_t;

/**
 * @brief Initialize a `po_args_t` structure with default values.
 * @param args Pointer to struct to initialize (must not be NULL).
 * @note Thread-safe: Yes (operates on stack/local memory).
 */
void po_args_init(po_args_t *args) __nonnull((1));

/**
 * @brief Parse command-line arguments into an initialized `po_args_t`.
 *
 * Recognized options (illustrative, may evolve):
 *  --help, --version, --config FILE, --log-level LEVEL, --syslog, --syslog-ident IDENT,
 *  --tui-demo
 *
 * @param args  Initialized arguments struct to populate.
 * @param argc  Argument count from `main`.
 * @param argv  Argument vector from `main`.
 * @param fd    Optional file descriptor used for early diagnostics (>=0) or -1.
 * @return 0 on success; non-zero on parse error. If `help` or `version` is
 *         set, the return may still be 0 (caller decides to exit early).
 * @note Thread-safe: Yes (operates on stack/local memory, assumes argv is constant).
 */
int po_args_parse(po_args_t *args, int argc, char **argv, int fd) __nonnull((1, 3));

/**
 * @brief Print usage information to the given file descriptor.
 * @param fd        File descriptor (e.g., STDOUT_FILENO / STDERR_FILENO).
 * @param prog_name Program name for usage banner (must not be NULL).
 * @note Thread-safe: Yes (reentrant).
 */
void po_args_print_usage(int fd, const char *prog_name) __nonnull((2));

/**
 * @brief Release any dynamically allocated fields inside the struct.
 * @param args Arguments struct to clean (must not be NULL). Safe to call multiple times.
 * @note Thread-safe: Yes, provided `args` is not accessed concurrently.
 */
void po_args_destroy(po_args_t *args) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // PO_UTILS_ARGV_H