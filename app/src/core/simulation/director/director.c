// Director process: manages simulation lifecycle, orchestration, and metrics.
#define _POSIX_C_SOURCE 200809L
#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>
#include <postoffice/log/logger.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <postoffice/net/socket.h>
#include <utils/errors.h>
#include "schedule/task_queue.h"
#include "ctrl_bridge/bridge_mainloop.h"
#include "ipc/simulation_ipc.h"         // Added IPC header

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <utils/signals.h>

static volatile sig_atomic_t g_running = 1;
static bool g_headless_mode = false;
static sim_shm_t* g_shm = NULL;         // Global SHM pointer

static void parse_args(int argc, char** argv) {
    static struct option long_options[] = {
        {"headless", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'c'},
        {"loglevel", required_argument, 0, 'l'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hc:l:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                g_headless_mode = true;
                break;
            case 'c':
                // Config handling
                break;
            case 'l':
                // TODO: Parse log level string to enum if needed
                break;
            default:
                break;
        }
    }
}

static void handle_signal(int sig, siginfo_t* info, void* context) {
    (void)sig; (void)info; (void)context;
    g_running = 0;
}

static void register_signals(void) {
    if (sigutil_handle_terminating(handle_signal, 0) != 0) {
        LOG_WARN("Failed to register signal handlers");
    }
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

    // 3. Initialize Shared Memory
    g_shm = sim_ipc_shm_create();
    if (!g_shm) {
        LOG_ERROR("director: failed to create shared memory (errno=%d)", errno);
        po_logger_shutdown();
        exit(EXIT_FAILURE);
    }
    LOG_INFO("Shared memory initialized at %p", (void*)g_shm);

    // 3.1 Initialize Semaphores
    int semid = sim_ipc_sem_create(SIM_MAX_SERVICE_TYPES);
    if (semid == -1) {
        LOG_ERROR("director: failed to create semaphores (errno=%d)", errno);
        sim_ipc_shm_destroy();
        po_logger_shutdown();
        exit(EXIT_FAILURE);
    }
    LOG_INFO("Semaphores initialized (ID: %d)", semid);

    // 4. Centralized IPC Creation
    // A. Shared Worker Logfile
    FILE* f_workers = fopen("logs/workers.log", "w"); // Truncate
    if (f_workers) fclose(f_workers);
    else LOG_WARN("Failed to truncate logs/workers.log");

    // B. Ticket Issuer Socket (Created by Director, Inherited by Issuer)
    int issuer_sock_fd = po_socket_listen_unix(SIM_SOCK_ISSUER, 128);
    if (issuer_sock_fd < 0) {
        LOG_ERROR("director: failed to bind issuer socket %s", SIM_SOCK_ISSUER);
        exit(1);
    }
    // Clear CLOEXEC so child inherits it
    int flags = fcntl(issuer_sock_fd, F_GETFD);
    fcntl(issuer_sock_fd, F_SETFD, flags & ~FD_CLOEXEC);
    LOG_INFO("Created Issuer Socket FD: %d", issuer_sock_fd);

    // C. Initialize Global Time
    atomic_store(&g_shm->time_control.day, 1);
    atomic_store(&g_shm->time_control.hour, 8); // Start at 08:00
    atomic_store(&g_shm->time_control.minute, 0);
    atomic_store(&g_shm->time_control.elapsed_nanos, 0);

    // 5. Process Spawning
    // Helper macro to spawn
    #define SPAWN(bin, ...) do { \
        pid_t p = fork(); \
        if (p == 0) { \
            execl(bin, bin, ##__VA_ARGS__, NULL); \
            LOG_ERROR("Failed to exec %s: %s", bin, strerror(errno)); \
            exit(1); \
        } else if (p < 0) { \
            LOG_ERROR("Failed to fork %s: %s", bin, strerror(errno)); \
        } else { \
            LOG_INFO("Spawned %s (PID %d)", bin, p); \
        } \
    } while(0)

    // Note: We assume binaries are in "bin/" relative to current CWD or absolute path.
    // The Makefile builds to bin/, logic runs from app/.
    // Binaries are: bin/post_office_ticket_issuer, bin/post_office_worker, bin/post_office_users_manager

    const char *bin_issuer = "bin/post_office_ticket_issuer";
    const char *bin_worker = "bin/post_office_worker";
    const char *bin_manager = "bin/post_office_users_manager";

    // A. Spawn Ticket Issuer (Pass FD)
    char fd_str[16];
    snprintf(fd_str, sizeof(fd_str), "%d", issuer_sock_fd);
    SPAWN(bin_issuer, "-l", "INFO", "--socket-fd", fd_str); 

    // B. Spawn Workers (e.g. 4 workers)
    for (int i = 0; i < 4; i++) {
        char id_str[8], type_str[8];
        snprintf(id_str, sizeof(id_str), "%d", i);
        snprintf(type_str, sizeof(type_str), "%d", i % SIM_MAX_SERVICE_TYPES);
        SPAWN(bin_worker, "-i", id_str, "-s", type_str);
    }

    // C. Spawn Users Manager
    char pid_str[16];
    snprintf(pid_str, sizeof(pid_str), "%d", getpid());
    SPAWN(bin_manager, "--pid", pid_str);

    // 6. Setup Task Queue (MPSC)
    po_task_queue_t task_queue;
    if (po_task_queue_init(&task_queue, 256) != 0) {
        LOG_ERROR("failed to init task queue");
        sim_ipc_shm_destroy();
        return 1;
    }

    // 7. Register Signals
    register_signals();

    // 8. Start Control Bridge
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

    LOG_INFO("Director started (PID: %d), Sim Time: Day %d %02d:%02d", 
             getpid(), 
             atomic_load(&g_shm->time_control.day), 
             atomic_load(&g_shm->time_control.hour), 
             atomic_load(&g_shm->time_control.minute));

    // Main Loop
    atomic_store(&g_shm->time_control.sim_active, true);
    
    g_running = true; 
    while (g_running) {
        // A. Child Monitoring
        int status;
        pid_t exited_pid = waitpid(-1, &status, WNOHANG);
        if (exited_pid > 0) {
            LOG_ERROR("Child process %d exited unexpectedly (status %d). Shutting down.", exited_pid, status);
            g_running = false;
            break;
        }

        // B. Simulation Tick (Time Advancement)
        int min = atomic_load(&g_shm->time_control.minute);
        int hour = atomic_load(&g_shm->time_control.hour);
        int day = atomic_load(&g_shm->time_control.day);

        min++;
        if (min >= 60) {
            min = 0;
            hour++;
            if (hour >= 24) { // Close logic could go here
                hour = 0;
                day++;
                LOG_INFO("New Day: %d", day);
            }
            if (hour % 4 == 0) { // Log every 4 hours
                LOG_INFO("Sim Time: Day %d %02d:00", day, hour);
            }
        }
        
        // Update SHM atomically
        atomic_store(&g_shm->time_control.minute, min);
        atomic_store(&g_shm->time_control.hour, hour);
        atomic_store(&g_shm->time_control.day, day);

        // C. Sleep
        usleep(10000); // 100ms per minute -> 2.4s per day roughly (24*60 / 10 = 144 ticks?) 
                       // Wait, 10000us = 10ms. 
                       // 10ms * 60 = 600ms = 1 hour.
                       // 600ms * 24 = 14.4s = 1 day. Good.
    }

    LOG_INFO("Director stopping...");

    // Disable simulation
    atomic_store(&g_shm->time_control.sim_active, false);

    // 9. Shutdown Sequence
    if (bridge_started) {
        bridge_mainloop_stop();
        pthread_join(bridge_thread, NULL);
    }
    
    // Kill all child processes (workers, users, issuer) in the process group
    kill(0, SIGTERM);
    // Give them a moment to shutdown and flush logs
    usleep(100000);

    // Cleanup children
    kill(0, SIGTERM); 
    usleep(100000);

    // Clean up Shared Memory
    if (g_shm) {
        sim_ipc_shm_detach(g_shm);
        sim_ipc_shm_destroy(); // Master destroys
        LOG_INFO("Shared memory destroyed");
    }

    // Final log before logger shutdown
    LOG_INFO("Director exit.");
    po_logger_shutdown();

    po_task_queue_destroy(&task_queue);
    po_metrics_shutdown();
    return 0;
}
