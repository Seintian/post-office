/**
 * @file director.h
 * @ingroup executables
 * @brief Simulation Director process – central coordinator orchestrating
 *        worker, user, ticket issuer and manager processes.
 *
 * Responsibilities
 * ----------------
 *  - Initializes shared IPC primitives (queues, pipes, sockets – see net layer).
 *  - Spawns subordinate processes (worker, users_manager, ticket_issuer, user instances).
 *  - Aggregates high-level metrics (if enabled) and forwards to logging.
 *  - Handles global shutdown sequencing (signals -> broadcast -> graceful join -> forced kill).
 *
 * Lifecycle
 * ---------
 * 1. Parse configuration / CLI.
 * 2. Initialize logger, perf/metrics, storage (optional).
 * 3. Spawn subordinate processes with negotiated descriptors.
 * 4. Enter supervision loop (monitor liveness, restart policy TBD).
 * 5. On termination signal: propagate orderly shutdown, flush metrics/storage, exit.
 *
 * Concurrency & IPC
 * -----------------
 * Single-threaded event / wait loop; relies on non-blocking I/O (epoll/poller) for coordination.
 *
 * Error Handling
 * --------------
 * Fatal initialization errors abort early with non-zero exit status; runtime failures may trigger
 * controlled restarts or cascade shutdown depending on configuration.
 */

#ifndef PO_CORE_DIRECTOR_H
#define PO_CORE_DIRECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PO_CORE_DIRECTOR_H */
