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

/* Minimal process descriptor and lifecycle helpers (skeleton).
 * This header intentionally contains only lightweight, header-only helpers
 * so that other Director code can reference process descriptors without
 * requiring a separate translation unit. It is a skeleton: add fields
 * and richer behaviors as implementation progresses.
 */

#include <sys/types.h>
#include <stdbool.h>
#include <errno.h>

typedef enum po_process_role {
	PO_ROLE_UNKNOWN = 0,
	PO_ROLE_DIRECTOR,
	PO_ROLE_WORKER,
	PO_ROLE_USER,
	PO_ROLE_TICKET_ISSUER,
	PO_ROLE_USERS_MANAGER,
} po_process_role_t;

typedef enum po_process_state {
	PO_PROC_INIT = 0,
	PO_PROC_RUNNING,
	PO_PROC_SUSPENDED,
	PO_PROC_TERMINATED,
} po_process_state_t;

struct po_process {
	pid_t pid;                 /* OS pid for child process */
	po_process_role_t role;    /* Role of the process */
	po_process_state_t state;  /* Current lifecycle state */
	/* future: per-process metrics, timestamps, flags */
};

/* Initialize a process descriptor. Returns 0 on success, -1 on failure. */
static inline int po_process_init(struct po_process* p, po_process_role_t role,
								  pid_t pid) {
	if (!p) { errno = EINVAL; return -1; }
	p->pid = pid;
	p->role = role;
	p->state = PO_PROC_INIT;
	return 0;
}

static inline pid_t po_process_pid(const struct po_process* p) {
	return p ? p->pid : (pid_t)-1;
}

static inline po_process_role_t po_process_role(const struct po_process* p) {
	return p ? p->role : PO_ROLE_UNKNOWN;
}

static inline bool po_process_is_active(const struct po_process* p) {
	return p && p->state == PO_PROC_RUNNING;
}

static inline void po_process_set_state(struct po_process* p, po_process_state_t s) {
	if (!p) return;
	p->state = s;
}

#endif /* PO_DIRECTOR_PROCESS_H */
