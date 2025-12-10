#include "utils/argv.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <postoffice/log/logger.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/sysinfo/sysinfo.h>
#include "tui/app_tui.h"
#include "simulation/simulation_lifecycle.h"

int main(int argc, char *argv[]) {
    po_args_t args;
    po_args_init(&args);
    int rc = po_args_parse(&args, argc, argv, STDERR_FILENO);
    if (rc != 0) {
        po_args_destroy(&args);
        return rc < 0;
    }

    // TUI Demo mode check from arguments (simplified for this task)
    // Assuming po_args_parse handles --tui-demo and we can check it
    // Or we scan argv manually if arg structure is unknown 
    // For safety, scanning argv manually to respect previous behavior
    bool tui_demo = false;
    bool tui_sim = false;
    for(int i=1; i<argc; i++) {
        if(strcmp(argv[i], "--tui-demo") == 0) {
            tui_demo = true;
        } else if(strcmp(argv[i], "--tui") == 0) {
            tui_sim = true;
        }
    }

    // Initialize simulation lifecycle
    simulation_init(args.config_file);

    if (tui_demo) {
        app_tui_run_demo();
        po_args_destroy(&args);
        return 0;
    }

    if (tui_sim) {
        // Start simulation processes
        simulation_start();

        // Run TUI
        app_tui_run_simulation();

        // Stop simulation when TUI exits
        simulation_stop();

        po_args_destroy(&args);
        return 0;
    }

    if (po_metrics_init() != 0) {
        fprintf(stderr, "metrics: init failed\n");
        po_args_destroy(&args);
        return 1;
    }

    // Initialize logger according to CLI
    po_logger_config_t cfg = {
        .level = (args.loglevel >= 0 && args.loglevel <= 5) ? (po_log_level_t)args.loglevel : LOG_INFO,
        .ring_capacity = 1u << 14, // 16384 entries
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
    };
    if (po_logger_init(&cfg) != 0) {
        fprintf(stderr, "logger: init failed\n");
        po_metrics_shutdown();
        po_args_destroy(&args);
        return 1;
    }

    // Always log to console by default
    po_logger_add_sink_console(true);
    if (args.syslog)
        po_logger_add_sink_syslog(args.syslog_ident);

    LOG_INFO("post-office main started (level=%d)%s", (int)po_logger_get_level(),
             args.syslog ? " with syslog" : "");

    // Collect and log system information
    po_sysinfo_t sysinfo;
    if (po_sysinfo_collect(&sysinfo) == 0) {
        // Log brief info
        LOG_INFO("System: %d physical cores, %ld MB RAM", sysinfo.physical_cores, sysinfo.total_ram / 1024 / 1024);
        // Print detailed info to stdout if running headless
        if (!tui_sim && !tui_demo) {
            po_sysinfo_print(&sysinfo, stdout);
        }
    } else {
        LOG_WARN("Failed to collect system information");
    }

    // Example metric usage at startup
    PO_METRIC_COUNTER_INC("app.start");

    // Headless execution
    LOG_INFO("Entering headless simulation mode...");
    simulation_run_headless();

    // Clean shutdown and exit
    po_logger_shutdown();
    po_metrics_shutdown();
    po_args_destroy(&args);
    return 0;
}
