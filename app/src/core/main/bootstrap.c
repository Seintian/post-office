/**
 * @file bootstrap.c
 * @brief Application initialization and teardown logic.
 */

#define _POSIX_C_SOURCE 200809L

#include "bootstrap.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <postoffice/backtrace/backtrace.h>
#include <postoffice/log/logger.h>
#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>
#include <postoffice/sysinfo/sysinfo.h>

static int initialize_metrics(void) {
    if (po_metrics_init(0, 0, 0) != 0) {
        fprintf(stderr, "metrics: init failed\n");
        return -1;
    }
    return 0;
}

static int initialize_logger(int loglevel, size_t cacheline_size, const po_args_t *args, bool is_tui) {
    po_logger_config_t cfg = {
        .level = (loglevel >= 0 && loglevel <= 5) ? (po_log_level_t)loglevel : LOG_INFO,
        .ring_capacity = 1u << 14, // 16384 entries
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = cacheline_size,
    };

    if (po_logger_init(&cfg) != 0) {
        fprintf(stderr, "logger: init failed\n");
        return -1;
    }

    // Sinks
    po_logger_add_sink_file("logs/main.log", false); // overwrite
    if (!is_tui) {
        po_logger_add_sink_console(true);
    }
    if (args->syslog) {
        po_logger_add_sink_syslog(args->syslog_ident);
    }

    return 0;
}

int app_bootstrap_system(const po_args_t *args, bool *out_is_tui) {
    // 1. Determine Mode
    if (out_is_tui) {
        *out_is_tui = args->tui_demo || args->tui_sim;
    }
    bool is_tui = (args->tui_demo || args->tui_sim);

    // 2. Collect System Info (for optimization params)
    po_sysinfo_t sysinfo;
    size_t cacheline_size = 64; 
    if (po_sysinfo_collect(&sysinfo) == 0 && sysinfo.dcache_lnsize > 0) {
        cacheline_size = (size_t)sysinfo.dcache_lnsize;
    }

    // 3. Initialize Subsystems
    if (initialize_metrics() != 0) return -1;
    if (initialize_logger(args->loglevel, cacheline_size, args, is_tui) != 0) return -1;
    
    backtrace_init("crash_reports");

    // 4. Start Background Samplers
    if (po_sysinfo_sampler_init() != 0) {
        LOG_WARN("Failed to start system info sampler");
    }

    LOG_INFO("post-office main started (level=%d)%s", 
             (int)po_logger_get_level(),
             args->syslog ? " with syslog" : "");

    return 0;
}

void app_log_system_info(bool is_tui) {
    (void)is_tui;
    po_sysinfo_t sysinfo;
    if (po_sysinfo_collect(&sysinfo) == 0) {
        LOG_DEBUG("=== System Information ===");
        LOG_DEBUG("Logical Processors: %ld", sysinfo.logical_processors);
        LOG_DEBUG("Total RAM: %.2f GB", (double)sysinfo.total_ram / (1024.0 * 1024.0 * 1024.0));
        LOG_DEBUG("Hostname: %s", sysinfo.hostname);
        LOG_DEBUG("CPU Model: %s", sysinfo.cpu_brand);
        LOG_DEBUG("Page Size: %ld bytes", sysinfo.page_size);
        LOG_DEBUG("Max Open Files: %lu", sysinfo.max_open_files);
        LOG_DEBUG("=========================");
    }
}

void app_shutdown_system(po_args_t *args) {
    LOG_INFO("Cleaning up resources and shutting down");
    po_sysinfo_sampler_stop();
    po_logger_shutdown();
    po_perf_shutdown(NULL);
    po_metrics_shutdown();
    po_args_destroy(args);
}
