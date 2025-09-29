/**
 * @file worker.h
 * @ingroup executables
 * @brief Worker process – executes queued tasks / simulated workload units.
 *
 * Responsibilities
 * ----------------
 *  - Consumes jobs / tickets issued by the ticket issuer or user interactions.
 *  - Performs CPU / I/O simulation steps (potentially instrumented via metrics).
 *  - Reports completion / status updates back to director or users_manager channel.
 *
 * Scheduling Model
 * ----------------
 * Typically event-driven; may poll a ring buffer, IPC channel, or socket; avoids blocking to
 * sustain high throughput.
 *
 * Shutdown
 * --------
 * Receives termination signal / control message, drains in-flight tasks (bounded), writes final
 * metrics snapshot, and exits.
 */

#ifndef PO_CORE_WORKER_H
#define PO_CORE_WORKER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PO_CORE_WORKER_H */
