/** \file process.h
 *  \ingroup director
 *  \brief Encapsulates lifecycle management of a simulated process entity
 *         (creation, activation, suspension, termination) within the Director.
 *
 *  Responsibilities
 *  ----------------
 *  - Define process descriptor structure (PID, role, state, stats snapshot).
 *  - Provide validation & transitions bridging state_model.h enumerations.
 *  - Surface lightweight accessors for hot-path queries (is_active, role).
 *  - Emit change events to event_log_sink.h for observability & UI updates.
 *
 *  Concurrency
 *  -----------
 *  Mutated only on Director thread; other threads read immutable copies or
 *  snapshots exported through state_store.h. No internal locking required.
 *
 *  Error Handling
 *  --------------
 *  Initialization / insertion failures bubble errno (ENOMEM, EEXIST). State
 *  transition violations return -1 (errno=EINVAL) and log diagnostics.
 *
 *  Extensibility
 *  -------------
 *  Additional per-process metrics or flags should group by cache locality;
 *  consider padding or reordering to avoid false sharing if shared memory
 *  export is added later.
 *
 *  Future Work
 *  -----------
 *  - Quiescence handshake for graceful termination.
 *  - Process priority hints feeding scheduler fairness heuristics.
 */
#ifndef PO_DIRECTOR_PROCESS_H
#define PO_DIRECTOR_PROCESS_H

/* Placeholder: struct po_process; API surface to be defined. */

#endif /* PO_DIRECTOR_PROCESS_H */
