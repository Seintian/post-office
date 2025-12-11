#include <utils/argv.h>
#include <stdio.h>
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

    // Manual argv scanning removed as po_args_parse now handles --tui
    bool tui_demo = args.tui_demo;
    bool tui_sim = args.tui_sim;

    // Initialize simulation lifecycle
    simulation_init(args.config_file);

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

    // Configure sinks based on mode
    bool is_tui = tui_sim || tui_demo;

    if (is_tui) {
        // In TUI mode, log to file to preserve screen for UI
        po_logger_add_sink_file("logs/postoffice.log", false); // overwrite
    } else {
        // In headless mode, log to console
        po_logger_add_sink_console(true);
    }

    if (args.syslog)
        po_logger_add_sink_syslog(args.syslog_ident);

    // Initialize metrics early so startup logs can effectively use them if needed
    // (though main process might not produce much metrics data, consistency is good)
    if (po_metrics_init() != 0) {
        fprintf(stderr, "metrics: init failed\n");
        po_args_destroy(&args);
        return 1;
    }

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

    if (tui_demo) {
        app_tui_run_demo();
        po_metrics_shutdown();
        po_logger_shutdown();
        po_args_destroy(&args);
        return 0;
    }

    if (tui_sim) {
        // Start simulation processes
        simulation_start(true);

        // Run TUI
        app_tui_run_simulation();

        // Stop simulation when TUI exits
        simulation_stop();

        po_metrics_shutdown();
        po_logger_shutdown();
        // The following lines appear to be from a different context (e.g., a logger consumer loop)
        // and cannot be directly inserted here in main.c without causing syntax errors or
        // referencing undeclared variables.
        // if (r == &g_sentinel)
        //         continue; // shutdown wake
        //
        //     // fprintf(stderr, "DEBUG: Processing record\n");
        //     write_record(r);
        //     PO_METRIC_COUNTER_INC("logger.processed");
        po_args_destroy(&args);
        return 0;
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
