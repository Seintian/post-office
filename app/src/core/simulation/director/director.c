// Director process: manages simulation lifecycle, orchestration, and metrics.
#define _POSIX_C_SOURCE 200809L
#include <postoffice/metrics/metrics.h>
#include <postoffice/perf/perf.h>
#include <postoffice/log/logger.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <postoffice/net/socket.h>
#include <postoffice/vector/vector.h>
#include <utils/errors.h>
#include "schedule/task_queue.h"
#include "ctrl_bridge/bridge_mainloop.h"
#include "ipc/simulation_ipc.h"

#include "utils/configs.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <utils/signals.h>
#include <stdatomic.h> // For atomic_load/store

static volatile sig_atomic_t g_running = 1;
static bool g_headless_mode = false;
static sim_shm_t* g_shm = NULL;         // Global SHM pointer

static po_vector_t *g_child_pids = NULL;

static uint32_t g_n_workers = DEFAULT_WORKERS;
static const char* g_config_path = NULL; // Global config path
static int g_n_users_initial = 5; // Initial users count
static int g_n_users_batch = 5; // Add/remove batch size
static const char* g_log_level_str = "INFO"; //Log level string

// Define constants for default start time
#define DEFAULT_START_DAY 1
#define DEFAULT_START_HOUR 0

// Define a cache line max for logger config
// #define PO_CACHE_LINE_MAX 64 // Using included definition

static void parse_args(int argc, char** argv) {
    static struct option long_options[] = {
        {"headless", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'c'},
        {"loglevel", required_argument, 0, 'l'},
        {"workers", required_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hc:l:w:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                g_headless_mode = true;
                break;
            case 'c':
                g_config_path = optarg; // Store config path
                break;
            case 'l':
                // Check if valid level
                if (po_logger_level_from_str(optarg) == -1) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    // fallback to INFO
                } else {
                    g_log_level_str = optarg;
                    setenv("PO_LOG_LEVEL", optarg, 1);
                }
                break;
            case 'w': {
                char *endptr;
                long val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val > 0) {
                    g_n_workers = (uint32_t)val;
                } else {
                    fprintf(stderr, "Invalid worker count: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            }
            default:
                break;
        }
    }
}

static void handle_signal(int sig, siginfo_t* info, void* context) {
    (void)sig; (void)info; (void)context;
    g_running = 0;
}

static void* bridge_thread_fn(void* _) {
    (void) _;
    bridge_mainloop_run();
    return NULL;
}

static void track_child(pid_t pid) {
    if (g_child_pids) {
        po_vector_push(g_child_pids, (void*)(intptr_t)pid);
    }
}

static void spawn_process(const char* bin, char *const argv[]) {
    pid_t p = fork();
    if (p == 0) {
        // Child
        execv(bin, argv);

        // If execv fails, use unsafe write to stderr to avoid lock corruption
        const char *msg = "Failed to exec process\n";
        if (write(STDERR_FILENO, msg, strlen(msg)) == -1) { /* ignore */ }
        _exit(1); 
    } else if (p < 0) {
        LOG_ERROR("Failed to fork %s: %s", bin, strerror(errno));
    } else {
        LOG_INFO("Spawned %s (PID %d)", bin, p);
        track_child(p);
    }
}

static void load_config_into_shm(const char *config_path) {
    po_config_t *cfg = NULL;
    // Load config (instrumented inside)
    if (po_config_load(config_path, &cfg) != 0) {
        LOG_WARN("Failed to load config %s, using defaults", config_path);
        // Defaults
        g_shm->params.sim_duration_days = 10;
        g_shm->params.tick_nanos = 1000000;
        g_shm->params.explode_threshold = 100;
        return;
    }

    // [simulation] SIM_DURATION
    int duration = 0;
    if (po_config_get_int(cfg, "simulation", "SIM_DURATION", &duration) == 0) {
        g_shm->params.sim_duration_days = (uint32_t)duration;
    } else {
        g_shm->params.sim_duration_days = 10; // default
    }

    // [simulation] N_NANO_SECS
    long tick_nanos = 0;
    if (po_config_get_long(cfg, "simulation", "N_NANO_SECS", &tick_nanos) == 0) {
        g_shm->params.tick_nanos = (uint64_t)tick_nanos;
    } else {
        g_shm->params.tick_nanos = 2500000;
    }

    // [simulation] EXPLODE_THRESHOLD
    int explode = 0;
    if (po_config_get_int(cfg, "simulation", "EXPLODE_THRESHOLD", &explode) == 0) {
        g_shm->params.explode_threshold = (uint32_t)explode;
    } else {
        g_shm->params.explode_threshold = 100;
    }
    
    // [workers] NOF_WORKERS
    int n_workers = 0;
    if (po_config_get_int(cfg, "workers", "NOF_WORKERS", &n_workers) == 0 && n_workers > 0) {
        // We might want to use this, but for now we rely on CLI or default.
        // If we want config to override, we need to restructure main().
        // For now, let's just log it.
    } 

    // [users_manager] N_NEW_USERS (batch size for add/remove)
    int n_new_users = 0;
    if (po_config_get_int(cfg, "users_manager", "N_NEW_USERS", &n_new_users) == 0 && n_new_users > 0) {
        g_n_users_batch = n_new_users;
        g_n_users_initial = n_new_users; // Also use as initial count
        LOG_INFO("Config: Users initial=%d, batch=%d", g_n_users_initial, g_n_users_batch);
    }

    // [users]
    int n_requests = 0;
    if (po_config_get_int(cfg, "users", "N_REQUESTS", &n_requests) == 0 && n_requests > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", n_requests);
        setenv("PO_USER_REQUESTS", buf, 1);
        LOG_INFO("Config: Set PO_USER_REQUESTS to %d", n_requests);
    }
    
    // Probabilities are doubles/floats. config parser supports string/int/bool? 
    // Need to check po_config API or use get_string and strtod.
    // Assuming po_config_get_string exists or similar.
    // Checking previous context: only get_int, get_long in snippet.
    // Let's assume get_string is available or use get_int for now if scaled? 
    // They are probabilities 0.5. So likely string.
    const char* p_min = NULL;
    if (po_config_get_str(cfg, "users", "P_SERV_MIN", &p_min) == 0 && p_min) {
        setenv("PO_USER_P_MIN", p_min, 1);
    }

    const char* p_max = NULL;
    if (po_config_get_str(cfg, "users", "P_SERV_MAX", &p_max) == 0 && p_max) {
        setenv("PO_USER_P_MAX", p_max, 1);
    }


    LOG_INFO("Loaded Config: Duration=%u days, Tick=%lu ns, Explode=%u",
             g_shm->params.sim_duration_days, g_shm->params.tick_nanos, g_shm->params.explode_threshold);

    po_config_free(&cfg);
}

// Helper to update SHM time
static void update_sim_time(sim_shm_t* shm, int day, int hour, int min) {
    uint64_t new_packed = ((uint64_t)day << 16) | ((uint64_t)hour << 8) | (uint64_t)min;
    atomic_store(&shm->time_control.packed_time, new_packed);
}

int main(int argc, char** argv) {
    // Parse command-line arguments
    parse_args(argc, argv);
    
    struct sigaction sa;
    sa.sa_sigaction = handle_signal;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // 0. Collect system information for optimizations
    /* po_sysinfo_t sysinfo;
    if (po_sysinfo_collect(&sysinfo) == 0 && sysinfo.dcache_lnsize > 0) {
         // cacheline_size = (size_t)sysinfo.dcache_lnsize; 
    } */
    
    // Initialize PID vector
    g_child_pids = po_vector_create();
    if (!g_child_pids) {
        fprintf(stderr, "Failed to allocate PID vector\n");
        exit(EXIT_FAILURE);
    }

    // 1. Initialize Metrics Subsystem (which initializes perf internally)
    if (po_metrics_init() != 0) {
        fprintf(stderr, "director: metrics init failed: %s\n", po_strerror(errno));
    }

    // 2. Initialize Logger with optimal cache line size
    int initial_level = LOG_INFO;
    char *env_lvl = getenv("PO_LOG_LEVEL");
    if (env_lvl) {
        int l = po_logger_level_from_str(env_lvl);
        if (l != -1) initial_level = l;
    }

    po_logger_config_t log_cfg = {
        .level = initial_level,
        .ring_capacity = 4096,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
        .cacheline_bytes = PO_CACHE_LINE_MAX
    };
    if (po_logger_init(&log_cfg) != 0) {
         fprintf(stderr, "Failed to init logger\n");
         return 1;
    }
    // Add default sinks
    po_logger_add_sink_console(true);
    po_logger_add_sink_file("logs/director.log", false);

    // Init Perf early
    if (po_perf_init(64, 16, 8) != 0) {
        LOG_WARN("Failed to init perf: %s", strerror(errno));
    }

    // Cleanup stale SHM/Semaphores from previous runs if any
    sim_ipc_shm_destroy();
    
    // Initialize Simulation IPC (Shared Memory)
    // Use n_workers from arguments (default 6)
    // NOTE: If we want config-based worker count, we should parse config HERE, before SHM size calc.
    // For now, assume CLI provides workers or defaults.
    
    g_shm = sim_ipc_shm_create(g_n_workers);
    if (!g_shm) {
        LOG_FATAL("Failed to create shared memory: %s", strerror(errno));
        // Cleanup resources
        if (g_child_pids) po_vector_destroy(g_child_pids);
        po_logger_shutdown();
        exit(EXIT_FAILURE);
    }
    
    // Store n_workers in params for other processes
    // (Already done in sim_ipc_shm_create, but explicit check doesn't hurt)
    if (g_shm->params.n_workers != g_n_workers) {
        LOG_WARN("Director: SHM worker count mismatch? Expected %u vs %u", g_n_workers, g_shm->params.n_workers);
    }
    LOG_INFO("Shared memory initialized at %p", (void*)g_shm);

    // Now load config into parameters
    load_config_into_shm(g_config_path ? g_config_path : "config/timeout.ini");

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
    FILE* f_workers = fopen("logs/workers.log", "w"); 
    if (f_workers) fclose(f_workers);
    else LOG_WARN("Failed to truncate logs/workers.log");

    // Secure socket path logic
    const char* user_home = getenv("HOME");
    char sock_path[512];
    if (user_home) {
        snprintf(sock_path, sizeof(sock_path), "%s/.postoffice/issuer.sock", user_home);
        // Ensure dir exists
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "mkdir -p %s/.postoffice", user_home);
        if (system(cmd) == -1) { /* ignore */ }
    } else {
        snprintf(sock_path, sizeof(sock_path), "/tmp/postoffice_%d_issuer.sock", getuid()); // Fallback with UID
    }
    unlink(sock_path); // remove old if exists

    int issuer_sock_fd = po_socket_listen_unix(sock_path, 128);
    if (issuer_sock_fd < 0) {
        LOG_ERROR("director: failed to bind issuer socket %s", sock_path);
        sim_ipc_shm_destroy();
        po_logger_shutdown();
        exit(EXIT_FAILURE);
    }
    // Clear CLOEXEC
    int flags = fcntl(issuer_sock_fd, F_GETFD);
    if (flags != -1) fcntl(issuer_sock_fd, F_SETFD, flags & ~FD_CLOEXEC);

    LOG_INFO("Created Issuer Socket FD: %d at %s", issuer_sock_fd, sock_path);

    // 5. Process Spawning
    const char *bin_issuer = "bin/post_office_ticket_issuer";
    const char *bin_worker = "bin/post_office_worker";
    const char *bin_manager = "bin/post_office_users_manager";

    // Spawn Ticket Issuer
    char fd_str[16];
    snprintf(fd_str, sizeof(fd_str), "%d", issuer_sock_fd);
    char *args_issuer[] = {(char*)bin_issuer, "-l", (char*)g_log_level_str, "--socket-fd", fd_str, NULL};
    spawn_process(bin_issuer, args_issuer);

    // 6. Spawn Workers
    LOG_INFO("Spawning %u Worker processes...", g_n_workers);
    for (uint32_t i = 0; i < g_n_workers; i++) {
        char id_buf[16];
        snprintf(id_buf, sizeof(id_buf), "%u", i);

        // Assign service type round-robin
        int stype = i % SIM_MAX_SERVICE_TYPES;
        char stype_buf[16];
        snprintf(stype_buf, sizeof(stype_buf), "%d", stype);
        
        char *args_worker[] = {(char*)bin_worker, "-l", (char*)g_log_level_str, "-i", id_buf, "-s", stype_buf, NULL};
        spawn_process(bin_worker, args_worker);
    }

    // Spawn Users Manager
    char initial_str[16], batch_str[16];
    snprintf(initial_str, sizeof(initial_str), "%d", g_n_users_initial);
    snprintf(batch_str, sizeof(batch_str), "%d", g_n_users_batch);
    char *args_manager[] = {(char*)bin_manager, "-l", (char*)g_log_level_str, "--initial", initial_str, "--batch", batch_str, NULL};
    spawn_process(bin_manager, args_manager);

    po_task_queue_t task_queue;
    if (po_task_queue_init(&task_queue, 256) != 0) {
        LOG_ERROR("failed to init task queue");
        // We should kill children here properly
        kill(0, SIGTERM);
        return 1;
    }

    // register_signals(); // Replaced by direct sigaction calls above

    pthread_t bridge_thread;
    int bridge_started = 0;
    if (!g_headless_mode) {
        if (bridge_mainloop_init() == 0) {
            if (pthread_create(&bridge_thread, NULL, bridge_thread_fn, NULL) == 0) {
                bridge_started = 1;
                LOG_INFO("Control bridge started (TUI mode)");
            }
        }
    }

    // Parse initial time
    // uint64_t packed = atomic_load(&g_shm->time_control.packed_time);
    // int day = (packed >> 16) & 0xFFFF;
    // int hour = (packed >> 8) & 0xFF;
    // int min = packed & 0xFF;

    // LOG_INFO("Director started (PID: %d), Sim Time: Day %d %02d:%02d", getpid(), day, hour, min);

    // 7. Main Loop (Manage Time)
    // We need 1 thread/process updating time? 
    // Director IS the timekeeper.
    
    // Set active
    atomic_store(&g_shm->time_control.sim_active, true);
    
    int current_day = DEFAULT_START_DAY;
    int current_hour = DEFAULT_START_HOUR;
    int current_min = 0;
    
    LOG_INFO("Simulation started... (Ctrl+C to stop)");

    while (g_running) {
        // Update Time
        update_sim_time(g_shm, current_day, current_hour, current_min);

        // Sleep for 1 "tick" (1 simulated minute)
        // Convert tick_nanos to microseconds for usleep
        uint64_t sleep_us = g_shm->params.tick_nanos / 1000;
        if (sleep_us == 0) sleep_us = 1000; // minimum safety
        usleep((useconds_t)sleep_us);

        // Advance 1 minute
        current_min++;
        // Check limits before logging day change

        // 1. Timeout
        if (g_shm->params.sim_duration_days > 0 && 
            (uint32_t)current_day > g_shm->params.sim_duration_days) {
            LOG_INFO("TIMEOUT: Simulation duration reached (%d days). Stopping.", g_shm->params.sim_duration_days);
            g_running = 0;
            break;
        }

        if (current_min >= 60) {
            current_min = 0;
            current_hour++;
            if (current_hour >= 24) {
                current_day++;
                current_hour = 0;
                
                // Moved check above
                if (g_shm->params.sim_duration_days > 0 && 
                    (uint32_t)current_day > g_shm->params.sim_duration_days) {
                        // Will break next iter
                } else {
                    LOG_INFO("Day %d begin", current_day);
                }
            }
        }

        // 2. Explode (Queue Overflow)
        if (g_shm->params.explode_threshold > 0) {
            uint32_t total_waiting = 0;
            for (int i = 0; i < SIM_MAX_SERVICE_TYPES; i++) {
                total_waiting += atomic_load(&g_shm->queues[i].waiting_count);
            }

            if (total_waiting > g_shm->params.explode_threshold) {
                LOG_FATAL("EXPLODE: Too many waiting users (%u > %u). Simulation collapsed.", 
                        total_waiting, g_shm->params.explode_threshold);
                g_running = 0;
                break;
            }
        }

        // A. Child Monitoring
        // ... (existing child monitoring logic would be here if any)
    }


    LOG_INFO("Director stopping...");
    atomic_store(&g_shm->time_control.sim_active, false);

    if (bridge_started) {
        bridge_mainloop_stop();
        pthread_join(bridge_thread, NULL);
    }
    
    // Graceful Shutdown of children
    size_t count = po_vector_size(g_child_pids);
    for (size_t i=0; i<count; i++) {
        pid_t p = (pid_t)(intptr_t)po_vector_at(g_child_pids, i);
        if (p > 0) kill(p, SIGTERM);
    }
    // Final wait/cleanup
    usleep(100000);
    // Force kill if needed
    for (size_t i=0; i<count; i++) {
        pid_t p = (pid_t)(intptr_t)po_vector_at(g_child_pids, i);
        if (p > 0) waitpid(p, NULL, WNOHANG);
    }
    
    po_vector_destroy(g_child_pids);

    if (g_shm) {
        sim_ipc_shm_detach(g_shm);
        sim_ipc_shm_destroy(); 
        LOG_INFO("Shared memory destroyed");
    }

    LOG_INFO("Director exit.");
    po_logger_shutdown();
    po_task_queue_destroy(&task_queue);
    po_perf_shutdown(stdout);
    return 0;
}
