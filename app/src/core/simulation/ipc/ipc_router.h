/** \file ipc_router.h
 *  \ingroup director
 *  \brief Routes decoded IPC messages to target handlers (process control,
 *         state updates, telemetry ingestion) enforcing ordering guarantees
 *         where required.
 *
 *  Responsibilities
 *  ----------------
 *  - Maintain registration table: msg_type -> handler function.
 *  - Dispatch on Director thread or enqueue tasks (task_queue.h) if work
 *    should be deferred / batched.
 *  - Apply simple QoS (drop low-priority if backlog high) (future).
 *
 *  Concurrency
 *  -----------
 *  Registration occurs at init; dispatch single-threaded for simplicity.
 *  Future multi-threaded dispatch would require handler reentrancy audit.
 *
 *  Error Handling
 *  --------------
 *  Unknown message types increment a counter and optionally log at a sampled
 *  rate. Handler failures surfaced via return code for further action.
 */
#ifndef PO_DIRECTOR_IPC_ROUTER_H
#define PO_DIRECTOR_IPC_ROUTER_H

/* Placeholder for router struct & API (register, dispatch). */

#endif /* PO_DIRECTOR_IPC_ROUTER_H */
