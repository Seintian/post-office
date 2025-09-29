/**
 * @file users_manager.h
 * @ingroup executables
 * @brief Users Manager – supervises multiple user processes and aggregates their results.
 *
 * Responsibilities
 * ----------------
 *  - Spawns / monitors a configured pool of user processes.
 *  - Collects completion statistics (success/failure counts, latency summaries).
 *  - Applies restart/backoff policy for crashed users (if enabled).
 *
 * Data Flow
 * ---------
 * Receives per-user telemetry over IPC channels (pipes / sockets) and may forward condensed
 * metrics to director or metrics subsystem.
 */

#ifndef PO_CORE_USERS_MANAGER_H
#define PO_CORE_USERS_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PO_CORE_USERS_MANAGER_H */
