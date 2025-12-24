/**
 * @file main.c
 * @brief Main Entry Point for the Post Office Application.
 * 
 * Orchestrates initialization, mode selection (Headless vs TUI), 
 * and cleanup using helper modules.
 */

#define _POSIX_C_SOURCE 200809L

#include "bootstrap.h"
#include "simulation/simulation_lifecycle.h"
#include "tui/app_tui.h"

#include <stdio.h>
#include <unistd.h>
#include <postoffice/log/logger.h>
#include <postoffice/metrics/metrics.h> // For generic metric counters

static int process_command_line_arguments(po_args_t *args, int argc, char *argv[]) {
    po_args_init(args);
    int rc = po_args_parse(args, argc, argv, STDERR_FILENO);
    if (rc != 0) {
        po_args_destroy(args);
    }
    return rc;
}

static int execute_interactive_demo_mode(po_args_t *args) {
    app_tui_run_demo();
    app_shutdown_system(args);
    return 0;
}

static int execute_interactive_simulation_mode(po_args_t *args) {
    launch_simulation_process(true, (int)po_logger_get_level());
    app_tui_run_simulation();
    terminate_simulation_process();
    app_shutdown_system(args);
    return 0;
}

static int execute_headless_simulation_mode(po_args_t *args) {
    LOG_INFO("Entering headless simulation mode...");
    PO_METRIC_COUNTER_INC("app.start");
    execute_simulation_headless_mode();
    app_shutdown_system(args);
    return 0;
}

int main(int argc, char *argv[]) {
    po_args_t args;
    if (process_command_line_arguments(&args, argc, argv) != 0) {
        return 1;
    }

    bool is_tui = false;
    if (app_bootstrap_system(&args, &is_tui) != 0) {
        po_args_destroy(&args);
        return 1;
    }

    // Initialize logic layer
    initialize_simulation_configuration(args.config_file);
    
    app_log_system_info(is_tui);

    if (args.tui_demo) return execute_interactive_demo_mode(&args);
    if (args.tui_sim)  return execute_interactive_simulation_mode(&args);
    
    return execute_headless_simulation_mode(&args);
}
