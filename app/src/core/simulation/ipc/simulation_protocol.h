#ifndef PO_SIMULATION_PROTOCOL_H
#define PO_SIMULATION_PROTOCOL_H

#include <postoffice/perf/cache.h> // For PO_CACHE_LINE_MAX
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/**
 * @file simulation_protocol.h
 * @brief Shared definitions for Simulation IPC (Shared Memory & Sockets).
 * @ingroup simulation
 */

/* --- Constants --- */

#define SIM_SHM_NAME "/postoffice_shm"
// Socket path will be determined at runtime (e.g. $HOME/.postoffice/launcher.sock)
// Semaphore key will be derived via ftok

// SIM_MAX_WORKERS replaced by dynamic sizing in sim_params_t
#define DEFAULT_WORKERS 6
#define SIM_MAX_SERVICE_TYPES 4

#define DEFAULT_START_DAY 1
#define DEFAULT_START_HOUR 0

/* --- Typedefs --- */

/**
 * @brief Service types available in the simulation.
 */
typedef enum {
    SERVICE_TYPE_A = 0,
    SERVICE_TYPE_B = 1,
    SERVICE_TYPE_C = 2,
    SERVICE_TYPE_D = 3,
    SERVICE_TYPE_COUNT = 4
} service_type_t;

/**
 * @brief Status of a single worker.
 */
typedef enum {
    WORKER_STATUS_OFFLINE = 0,
    WORKER_STATUS_FREE,
    WORKER_STATUS_BUSY,
    WORKER_STATUS_PAUSED
} worker_state_t;

/**
 * @brief Structure representing a worker's live status in SHM.
 * Aligned to cache line to avoid false sharing.
 */
typedef struct __attribute__((aligned(PO_CACHE_LINE_MAX))) worker_status_s {
    atomic_int state;           // worker_state_t
    atomic_uint current_ticket; // Ticket # being served
    atomic_int service_type;    // Current service type
    atomic_int pid;             // Worker PID (atomic access)
} worker_status_t;
// Compile-time check to ensure no false sharing (size must be multiple of cache line)
_Static_assert(sizeof(worker_status_t) % PO_CACHE_LINE_MAX == 0, "worker_status_t size mismatch");

/**
 * @brief Structure representing a queue's live status in SHM.
 * Aligned to cache line.
 */
typedef struct __attribute__((aligned(PO_CACHE_LINE_MAX))) queue_status_s {
    atomic_uint waiting_count; // Users currently in queue
    atomic_uint total_served;  // Cumulative users served

    // Synchronization for this queue
    pthread_mutex_t mutex;
    pthread_cond_t cond_added;   // For workers to wait for tickets
    pthread_cond_t cond_served;  // For users to wait for completion

    // Ticket Queue for User->Worker handoff
    atomic_uint head;
    atomic_uint tail;
    atomic_uint tickets[128]; // 0 = empty, val = ticket+1
} queue_status_t;
_Static_assert(sizeof(queue_status_t) % PO_CACHE_LINE_MAX == 0, "queue_status_t size mismatch");

/**
 * @brief Global statistics (counters).
 * Aligned to cache line.
 */
typedef struct __attribute__((aligned(PO_CACHE_LINE_MAX))) global_stats_s {
    atomic_uint total_tickets_issued;
    atomic_uint total_services_completed;
    atomic_uint total_users_spawned;
    // Add more granular stats here
} global_stats_t;
_Static_assert(sizeof(global_stats_t) % PO_CACHE_LINE_MAX == 0, "global_stats_t size mismatch");

/**
 * @brief Configuration parameters (read-only after init).
 */
typedef struct sim_params_s {
    uint32_t n_workers;
    uint32_t sim_duration_days;
    uint32_t explode_threshold;
    uint64_t tick_nanos; // Nanoseconds per simulation minute
} sim_params_t;

/**
 * @brief Global Time & Control.
 *
 * We pack day (16 bits), hour (8 bits), minute (8 bits) into a single 64-bit atomic
 * to ensure readers always see a consistent snapshot of time.
 *
 * bits 0-7:   minute (0-59)
 * bits 8-15:  hour (0-23)
 * bits 16-31: day (1-65535)
 * bits 32-63: reserved / epoch counter
 */
typedef struct __attribute__((aligned(PO_CACHE_LINE_MAX))) sim_time_s {
    atomic_uint_least64_t packed_time;
    atomic_int elapsed_nanos; // Accumulator for minute steps
    atomic_bool sim_active;   // Global run flag
    
    // Time Synchronization
    pthread_mutex_t mutex;
    pthread_cond_t cond_tick;
} sim_time_t;
_Static_assert(sizeof(sim_time_t) % PO_CACHE_LINE_MAX == 0, "sim_time_t size mismatch");

/**
 * @brief Main Shared Memory Structure.
 */
#include <pthread.h>

/**
 * @brief Main Shared Memory Structure.
 */
typedef struct __attribute__((aligned(PO_CACHE_LINE_MAX))) sync_control_s {
    atomic_int barrier_active;  // 1 = Waiting for sync, 0 = Running
    atomic_uint ready_count;    // Number of processes ready
    atomic_uint required_count; // Total expected processes
    atomic_uint day_seq;        // Current day sequence

    // Process-Shared Synchronization Primitives
    pthread_mutex_t mutex;
    pthread_cond_t cond_workers_ready; // Workers signal this
    pthread_cond_t cond_day_start;     // Director signals this
} sync_control_t;
_Static_assert(sizeof(sync_control_t) % PO_CACHE_LINE_MAX == 0, "sync_control_t size mismatch");

/**
 * @brief Main Shared Memory Structure.
 */
typedef struct simulation_shm_s {
    // 1. Configuration (Read-only after init)
    sim_params_t params;
    char _pad0[PO_CACHE_LINE_MAX - sizeof(sim_params_t)];

    // 2. Time & Control (Aligned)
    sim_time_t time_control;

    // 3. Global Sequence for Tickets
    atomic_uint ticket_seq;
    char _pad1[PO_CACHE_LINE_MAX - sizeof(atomic_uint)];

    // 4. Statistics
    global_stats_t stats;

    // 5. Synchronization
    sync_control_t sync;

    // 6. Live Data - Queues
    queue_status_t queues[SIM_MAX_SERVICE_TYPES];

    // 7. Live Data - Workers (Flexible Array Member)
    // Must be at the end.
    worker_status_t workers[];
} sim_shm_t;

/* --- Socket Messages --- */

/**
 * @brief Message types for Ticket Issuer Protocol.
 */
typedef enum {
    MSG_TYPE_TICKET_REQ = 0x10,
    MSG_TYPE_TICKET_RESP = 0x11,
    MSG_TYPE_ERR = 0xFF
} msg_type_t;

/**
 * @brief Request payload: "I want a ticket for this service"
 */
typedef struct msg_ticket_req_s {
    pid_t requester_pid;
    pid_t requester_tid;
    service_type_t service_type;
} msg_ticket_req_t;

/**
 * @brief Response payload: "Here is your ticket number"
 */
typedef struct msg_ticket_resp_s {
    uint32_t ticket_number;
    service_type_t assigned_service;
} msg_ticket_resp_t;

#endif // PO_SIMULATION_PROTOCOL_H
