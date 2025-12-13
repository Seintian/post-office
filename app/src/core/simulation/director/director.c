// Director process: manages simulation lifecycle, orchestration, and metrics.
#define _POSIX_C_SOURCE 200809L
#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>
#include <postoffice/log/logger.h>
#include <utils/errors.h>
#include "schedule/task_queue.h"
#include "ctrl_bridge/bridge_mainloop.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>

static volatile sig_atomic_t g_running = 1;

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

int main(void) {
    // 1. Initialize Performance & Metrics Subsystems (BEFORE Logger)
    if (po_perf_init(16, 8, 4) != 0) {
        fprintf(stderr, "director: perf init failed: %s\n", po_strerror(errno));
    }

    if (po_metrics_init() != 0) {
        fprintf(stderr, "director: metrics init failed: %s\n", po_strerror(errno));
    }

    // 0. Initialize Logger
    po_logger_config_t log_cfg = {
        .level = LOG_INFO,
        .ring_capacity = 4096,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = 0
    };
    if (po_logger_init(&log_cfg) != 0) {
        fprintf(stderr, "director: logger init failed\n");
    }
    // Log to console (stderr is standard for logs, but user said "console" so let's use stderr for now as it's unbuffered usually)
    po_logger_add_sink_console(true);

    // 2. Setup Task Queue (MPSC)
    po_task_queue_t task_queue;
    if (po_task_queue_init(&task_queue, 256) != 0) {
        LOG_ERROR("failed to init task queue");
        return 1;
    }

    // 3. Register Signals
    register_signals();

    // 4. Start Control Bridge in Background Thread
    pthread_t bridge_thread;
    int bridge_started = 0;
    if (bridge_mainloop_init() == 0) {
        void* bridge_thread_fn(void* _) {
            (void) _;
            bridge_mainloop_run();
            return NULL;
        }

        if (pthread_create(&bridge_thread, NULL, bridge_thread_fn, NULL) != 0) {
            LOG_ERROR("failed to start bridge thread");
        } else {
            bridge_started = 1;
        }
    } else {
        LOG_ERROR("bridge init failed; continuing without control bridge");
    }

    // 5. Instrumentation Setup
    po_perf_counter_create("director.lifetime.loops");
    po_perf_counter_create("director.lifetime.tasks");
    
    const uint64_t loop_bins[] = { 10, 100, 1000, 10000 }; // microseconds cases
    po_perf_histogram_create("director.loop_us", loop_bins, 4);

    LOG_INFO("Director started (PID: %d)", getpid());

    // 6. Main Loop
    po_perf_timer_create("director.loop_tick");

    while (g_running) {
        po_perf_timer_start("director.loop_tick");
        
        // A. Drain Tasks
        size_t processed = po_task_drain(&task_queue, 32);
        if (processed > 0) {
            po_perf_counter_add("director.lifetime.tasks", processed);
        }

        // B. Simulation Tick (Placeholder)
        // simulation_update();

        po_perf_timer_stop("director.loop_tick");
        po_perf_counter_inc("director.lifetime.loops");

        // C. Idle / Sleep
        usleep(10000); 
    }

    LOG_INFO("Director stopping...");

    // 7. Shutdown Sequence
    if (bridge_started) {
        bridge_mainloop_stop();
        pthread_join(bridge_thread, NULL);
    }
    
    po_task_queue_destroy(&task_queue);
    
    po_metrics_shutdown();
    po_perf_shutdown(stdout); // Prints final report to stdout (keep this as report, not log)
    
    // Final log before logger shutdown
    LOG_INFO("Director exit.");
    po_logger_shutdown();
    return 0;
}
