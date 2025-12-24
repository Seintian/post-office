#ifndef APP_BOOTSTRAP_H
#define APP_BOOTSTRAP_H

#include <stdbool.h>
#include <utils/argv.h>

/**
 * @brief Initializes core subsystems (Metrics, Logger, Backtrace, SysInfo).
 * 
 * @param args Parsed command-line arguments (for log level/syslog).
 * @param[out] out_is_tui Returns true if TUI mode is detected/enabled.
 * @return 0 on success, non-zero on failure.
 */
int app_bootstrap_system(const po_args_t *args, bool *out_is_tui);

/**
 * @brief Logs system information to the configured logger.
 * @param is_tui True if TUI is enabled (affects context logging).
 */
void app_log_system_info(bool is_tui);

/**
 * @brief Shuts down subsystems and cleans up resources.
 * 
 * @param args Args structure to destroy.
 */
void app_shutdown_system(po_args_t *args);

#endif
