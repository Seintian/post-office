/** \file state_model.h
 *  \ingroup director
 *  \brief Declarative model of simulation entities & global state transitions
 *         (finite state machines, invariants, derived metrics) consumed by
 *         the Director for decision making and UI projection.
 *
 *  Goals
 *  -----
 *  - Centralize all entity state enumerations & transition rules.
 *  - Provide validation helpers (is_valid_transition(from,to)).
 *  - Emit structured change events for telemetry / UI (event_log_sink.h).
 *
 *  Invariants
 *  ----------
 *  - No transition may skip intermediate mandatory states (e.g., CREATED →
 *    ACTIVE only; must not jump to TERMINATED without PASSING through a
 *    shutdown / draining state where applicable).
 *  - Illegal transitions trigger diagnostic logging and are rejected.
 *
 *  Concurrency
 *  -----------
 *  Mutations occur on the Director thread; readers (UI adapters, metrics
 *  exporter) access snapshots or derive summaries via state_store.h which
 *  provides safe iteration primitives (RCU-like or snapshot copy TBD).
 *
 *  Extensibility
 *  -------------
 *  Adding a new entity state requires updating:
 *    - Enumeration definitions here.
 *    - Transition table / validation logic.
 *    - UI mapping (colors / labels) in TUI adapters.
 *    - Telemetry export filters if state is externally visible.
 *
 *  Future Work
 *  -----------
 *  - Auto-generate transition graph for documentation.
 *  - Persist previous N transitions for debugging race conditions.
 *
 *  @see state_store.h for storage layer API.
 */
#ifndef PO_DIRECTOR_STATE_MODEL_H
#define PO_DIRECTOR_STATE_MODEL_H

/* Placeholder for enumerations & validation API. */

#endif /* PO_DIRECTOR_STATE_MODEL_H */
