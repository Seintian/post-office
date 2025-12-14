#ifndef PO_SIMULATION_PROTOCOL_H
#define PO_SIMULATION_PROTOCOL_H

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
#define SIM_SOCK_ISSUER "/tmp/postoffice_issuer.sock" // For Ticket Issuer
#define SIM_SEM_KEY 0x1234  // Key for SysV Semaphores (can be derived from path too)

#define SIM_MAX_WORKERS 32
#define SIM_MAX_SERVICE_TYPES 4

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
 * Aligned to 64 bytes to avoid false sharing.
 */
typedef struct __attribute__((aligned(64))) worker_status_s {
    atomic_int state;            // worker_state_t
    atomic_uint current_ticket;  // Ticket # being served
    atomic_int service_type;     // Current service type
    pid_t pid;
    char _pad[44];               // Padding to 64 bytes
} worker_status_t;

/**
 * @brief Structure representing a queue's live status in SHM.
 * Aligned to 64 bytes.
 */
typedef struct __attribute__((aligned(64))) queue_status_s {
    atomic_uint waiting_count;   // Users currently in queue
    atomic_uint total_served;    // Cumulative users served
    char _pad[56];
} queue_status_t;

/**
 * @brief Global statistics (counters).
 */
typedef struct global_stats_s {
    atomic_uint total_tickets_issued;
    atomic_uint total_services_completed;
    atomic_uint total_users_spawned;
    // Add more granular stats here
} global_stats_t;

/**
 * @brief Configuration parameters (read-only after init).
 */
typedef struct sim_params_s {
    uint32_t n_workers;
    uint32_t n_users_initial;
    uint32_t sim_duration_sec;
    // Add other config params
} sim_params_t;

/**
 * @brief Global Time & Control.
 */
typedef struct sim_time_s {
    atomic_int day;           // Current simulation day (1-based)
    atomic_int hour;          // Current hour (0-23)
    atomic_int minute;        // Current minute (0-59)
    atomic_int elapsed_nanos; // Accumulator for minute steps
    atomic_bool sim_active;   // Global run flag
} sim_time_t;

/**
 * @brief Main Shared Memory Structure.
 */
typedef struct simulation_shm_s {
    // 1. Time & Control (Aligned)
    sim_time_t time_control;

    // 2. Global Sequence for Tickets
    atomic_uint ticket_seq;
    char _pad2[60]; // atomic_uint is 4 bytes, pad to 64

    // 3. Statistics
    global_stats_t stats;

    // 4. Live Data
    worker_status_t workers[SIM_MAX_WORKERS];
    queue_status_t  queues[SIM_MAX_SERVICE_TYPES];
} sim_shm_t;

/* --- Socket Messages --- */

/**
 * @brief Message types for Ticket Issuer Protocol.
 */
typedef enum {
    MSG_TYPE_TICKET_REQ  = 0x10,
    MSG_TYPE_TICKET_RESP = 0x11,
    MSG_TYPE_ERR         = 0xFF
} msg_type_t;

/**
 * @brief Request payload: "I want a ticket for this service"
 */
typedef struct msg_ticket_req_s {
    pid_t requester_pid;
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
