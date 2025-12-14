// Director process: manages simulation lifecycle, orchestration, and metrics.
#define _POSIX_C_SOURCE 200809L
#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>
#include <postoffice/log/logger.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <utils/errors.h>
#include "schedule/task_queue.h"
#include "ctrl_bridge/bridge_mainloop.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>

static volatile sig_atomic_t g_running = 1;
static bool g_headless_mode = false;

static void parse_args(int argc, char** argv) {
    static struct option long_options[] = {
        {"headless", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'c'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hc:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                g_headless_mode = true;
                break;
            case 'c':
                // Config handling (if needed in future)
                break;
            default:
                break;
        }
    }
}

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

static void register_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    // Interrupt calls to wake up sleeping loops if possible
    sa.sa_flags = 0; 

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void* bridge_thread_fn(void* _) {
    (void) _;
    bridge_mainloop_run();
    return NULL;
}

int main(int argc, char** argv) {
    // Parse command-line arguments
    parse_args(argc, argv);

    // 0. Collect system information for optimizations
    po_sysinfo_t sysinfo;
    size_t cacheline_size = 64; // default
    if (po_sysinfo_collect(&sysinfo) == 0 && sysinfo.dcache_lnsize > 0) {
        cacheline_size = (size_t)sysinfo.dcache_lnsize;
    }

    // 1. Initialize Metrics Subsystem (which initializes perf internally)
    if (po_metrics_init() != 0) {
        fprintf(stderr, "director: metrics init failed: %s\n", po_strerror(errno));
    }

    // 2. Initialize Logger with optimal cache line size
    po_logger_config_t log_cfg = {
        .level = LOG_INFO,
        .ring_capacity = 4096,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = cacheline_size
    };
    if (po_logger_init(&log_cfg) != 0) {
        fprintf(stderr, "director: logger init failed\n");
    }
    // Log to console (stderr is standard for logs, but user said "console" so let's use stderr for now as it's unbuffered usually)
    po_logger_add_sink_console(true);
    // Log to file
    po_logger_add_sink_file("logs/director.log", false);

    // 2. Setup Task Queue (MPSC)
    po_task_queue_t task_queue;
    if (po_task_queue_init(&task_queue, 256) != 0) {
        LOG_ERROR("failed to init task queue");
        return 1;
    }

    // 3. Register Signals
    register_signals();

    // 4. Start Control Bridge in Background Thread (only in TUI mode)
    pthread_t bridge_thread;
    int bridge_started = 0;

    if (!g_headless_mode) {
        if (bridge_mainloop_init() == 0) {
            if (pthread_create(&bridge_thread, NULL, bridge_thread_fn, NULL) != 0) {
                LOG_ERROR("failed to start bridge thread");
            } else {
                bridge_started = 1;
                LOG_INFO("Control bridge started (TUI mode)");
            }
        } else {
            LOG_ERROR("bridge init failed; continuing without control bridge");
        }
    } else {
        LOG_INFO("Running in headless mode - control bridge disabled");
    }

    // 5. Instrumentation Setup
    PO_METRIC_COUNTER_CREATE("director.lifetime.loops");
    PO_METRIC_COUNTER_CREATE("director.lifetime.tasks");

    const uint64_t loop_bins[] = { 10, 100, 1000, 10000 }; // microseconds cases
    PO_METRIC_HISTO_CREATE("director.loop_us", loop_bins, 4);

    LOG_INFO("Director started (PID: %d)", getpid());

    // 6. Main Loop
    PO_METRIC_TIMER_CREATE("director.loop_tick");

    // Temporarily skip main loop when in headless mode
    g_running = !g_headless_mode;
    while (g_running) {
        PO_METRIC_TIMER_START("director.loop_tick");

        // A. Drain Tasks
        // size_t processed = po_task_drain(&task_queue, 32);
        // if (processed > 0) {
        //     PO_METRIC_COUNTER_ADD("director.lifetime.tasks", processed);
        // }

        // B. Simulation Tick (Placeholder)
        // simulation_update();

        PO_METRIC_TIMER_STOP("director.loop_tick");
        PO_METRIC_COUNTER_INC("director.lifetime.loops");

        // C. Idle / Sleep
        usleep(10000); 
    }

    LOG_INFO("Director stopping...");

    // 7. Shutdown Sequence
    if (bridge_started) {
        bridge_mainloop_stop();
        pthread_join(bridge_thread, NULL);
    }

    // Final log before logger shutdown
    LOG_INFO("Director exit.");
    po_logger_shutdown();

    po_task_queue_destroy(&task_queue);
    po_metrics_shutdown();
    return 0;
}
