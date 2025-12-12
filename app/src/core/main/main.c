#include <utils/argv.h>
#include <stdio.h>
#include <unistd.h>
#include <postoffice/log/logger.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/sysinfo/sysinfo.h>
#include "tui/app_tui.h"
#include "simulation/simulation_lifecycle.h"

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Parse command-line arguments and validate them.
 * 
 * @param args Output parameter to store parsed arguments
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, non-zero on failure
 */
static int parse_arguments(po_args_t *args, int argc, char *argv[]) {
    po_args_init(args);
    int rc = po_args_parse(args, argc, argv, STDERR_FILENO);
    if (rc != 0) {
        po_args_destroy(args);
        return rc < 0;
    }
    return 0;
}

/**
 * Initialize the logger with appropriate configuration.
 * 
 * @param args Parsed command-line arguments
 * @return 0 on success, non-zero on failure
 */
static int initialize_logger(const po_args_t *args) {
    po_logger_config_t cfg = {
        .level = (args->loglevel >= 0 && args->loglevel <= 5) ? (po_log_level_t)args->loglevel : LOG_INFO,
        .ring_capacity = 1u << 14, // 16384 entries
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
    };
    
    if (po_logger_init(&cfg) != 0) {
        fprintf(stderr, "logger: init failed\n");
        return 1;
    }

    return 0;
}

/**
 * Configure logger sinks based on execution mode.
 * 
 * @param args Parsed command-line arguments
 * @param is_tui True if running in TUI mode, false if headless
 */
static void configure_logger_sinks(const po_args_t *args, bool is_tui) {
    po_logger_add_sink_file("logs/main.log", false); // overwrite
    if (!is_tui) {
        // In headless mode, log to console, too
        po_logger_add_sink_console(true);
    }

    if (args->syslog) {
        po_logger_add_sink_syslog(args->syslog_ident);
    }
}

/**
 * Initialize the metrics system.
 * 
 * @return 0 on success, non-zero on failure
 */
static int initialize_metrics(void) {
    if (po_metrics_init() != 0) {
        fprintf(stderr, "metrics: init failed\n");
        return 1;
    }
    return 0;
}

/**
 * Collect and log exhaustive system information.
 * 
 * @param is_tui True if running in TUI mode (for context only)
 */
static void log_system_information(bool is_tui) {
    (void)is_tui;
    po_sysinfo_t sysinfo;
    if (po_sysinfo_collect(&sysinfo) == 0) {
        // Log comprehensive system information from po_sysinfo_t
        LOG_INFO("=== System Information ===");
        LOG_INFO("Physical Cores: %d", sysinfo.physical_cores);
        LOG_INFO("Logical Processors: %ld", sysinfo.logical_processors);
        LOG_INFO("L1 Cache: %ld bytes", sysinfo.cache_l1);
        LOG_INFO("L1 Data Cache Line Size: %ld bytes", sysinfo.dcache_lnsize);
        LOG_INFO("L1 Data Cache: %ld bytes", sysinfo.dcache_l1);
        LOG_INFO("L2 Cache: %ld bytes", sysinfo.cache_l2);
        LOG_INFO("L3 Cache: %ld bytes", sysinfo.cache_l3);
        LOG_INFO("Total RAM: %ld bytes (%.2f GB)", 
                 sysinfo.total_ram, (double)sysinfo.total_ram / (1024 * 1024 * 1024));
        LOG_INFO("Free RAM: %ld bytes (%.2f GB)", 
                 sysinfo.free_ram, (double)sysinfo.free_ram / (1024 * 1024 * 1024));
        LOG_INFO("Total Swap: %ld bytes (%.2f GB)",
                sysinfo.swap_total, (double)sysinfo.swap_total / (1024 * 1024 * 1024));
        LOG_INFO("Free Swap: %ld bytes (%.2f GB)",
                sysinfo.swap_free, (double)sysinfo.swap_free / (1024 * 1024 * 1024));
        LOG_INFO("Page Size: %ld bytes", sysinfo.page_size);
        LOG_INFO("Max Open Files: %lu", sysinfo.max_open_files);
        LOG_INFO("Max Processes: %lu", sysinfo.max_processes);
        LOG_INFO("Max Stack Size: %lu bytes", sysinfo.max_stack_size);
        LOG_INFO("Disk Free: %lu bytes", sysinfo.disk_free);
        LOG_INFO("Filesystem Type: %s", sysinfo.fs_type);
        LOG_INFO("Hostname: %s", sysinfo.hostname);
        LOG_INFO("CPU Vendor: %s", sysinfo.cpu_vendor);
        LOG_INFO("CPU Brand: %s", sysinfo.cpu_brand);
        LOG_INFO("CPU I/O Wait: %.2f%%", sysinfo.cpu_iowait_pct);
        LOG_INFO("CPU Utilization: %.2f%%", sysinfo.cpu_util_pct);
        LOG_INFO("MTU: %d", sysinfo.mtu);
        LOG_INFO("SOMAXCONN: %d", sysinfo.somaxconn);
        LOG_INFO("Little Endian: %d", sysinfo.is_little_endian);
        LOG_INFO("Huge Page Size: %lu KiB", sysinfo.hugepage_info.size_kB);
        LOG_INFO("Huge Pages Total: %ld", sysinfo.hugepage_info.nr);
        LOG_INFO("Huge Pages Free: %ld", sysinfo.hugepage_info.free);
        LOG_INFO("Huge Pages Overcommit: %ld", sysinfo.hugepage_info.overcommit);
        LOG_INFO("Huge Pages Surplus: %ld", sysinfo.hugepage_info.surplus);
        LOG_INFO("Huge Pages Reserved: %ld", sysinfo.hugepage_info.reserved);
        LOG_INFO("System Uptime: %lu seconds", sysinfo.uptime_seconds);
        LOG_INFO("Load Average (1 min): %.2f", sysinfo.load_avg_1min);
        LOG_INFO("Load Average (5 min): %.2f", sysinfo.load_avg_5min);
        LOG_INFO("Load Average (15 min): %.2f", sysinfo.load_avg_15min);
        LOG_INFO("=========================");
    } else {
        LOG_WARN("Failed to collect system information");
    }
}

/**
 * Log startup information and record startup metric.
 * 
 * @param args Parsed command-line arguments
 */
static void log_startup_info(const po_args_t *args) {
    LOG_INFO("post-office main started (level=%d)%s", 
             (int)po_logger_get_level(),
             args->syslog ? " with syslog" : "");
}

/**
 * Clean up all resources and prepare for exit.
 * 
 * @param args Parsed command-line arguments to destroy
 */
static void cleanup_resources(po_args_t *args) {
    /* Stop background samplers before shutting down other systems */
    po_sysinfo_sampler_stop();
    po_logger_shutdown();
    po_metrics_shutdown();
    po_args_destroy(args);
}

/**
 * Run TUI demo mode.
 * 
 * @param args Parsed command-line arguments
 * @return Exit code (always 0)
 */
static int run_tui_demo_mode(po_args_t *args) {
    app_tui_run_demo();
    cleanup_resources(args);
    return 0;
}

/**
 * Run TUI simulation mode.
 * 
 * @param args Parsed command-line arguments
 * @return Exit code (always 0)
 */
static int run_tui_simulation_mode(po_args_t *args) {
    // Start simulation processes
    simulation_start(true);

    // Run TUI
    app_tui_run_simulation();

    // Stop simulation when TUI exits
    simulation_stop();

    cleanup_resources(args);
    return 0;
}

/**
 * Run headless simulation mode.
 * 
 * @param args Parsed command-line arguments
 * @return Exit code (always 0)
 */
static int run_headless_mode(po_args_t *args) {
    LOG_INFO("Entering headless simulation mode...");
    PO_METRIC_COUNTER_INC("app.start");
    simulation_run_headless();
    cleanup_resources(args);
    return 0;
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char *argv[]) {
    po_args_t args;

    // Parse command-line arguments
    if (parse_arguments(&args, argc, argv) != 0) {
        return 1;
    }

    // Initialize logger
    if (initialize_logger(&args) != 0) {
        po_metrics_shutdown();
        po_args_destroy(&args);
        return 1;
    }

    // Initialize simulation lifecycle
    simulation_init(args.config_file);

    // Determine execution mode
    bool is_tui = args.tui_demo || args.tui_sim;

    // Configure logger sinks based on mode
    configure_logger_sinks(&args, is_tui);

    // Initialize metrics
    if (initialize_metrics() != 0) {
        po_args_destroy(&args);
        return 1;
    }

    /* Start background sampler for precise CPU metrics */
    (void)po_sysinfo_sampler_init();

    // Log startup information
    log_startup_info(&args);
    log_system_information(is_tui);

    // Dispatch to appropriate mode handler
    if (args.tui_demo) {
        return run_tui_demo_mode(&args);
    }

    if (args.tui_sim) {
        return run_tui_simulation_mode(&args);
    }

    return run_headless_mode(&args);
}
