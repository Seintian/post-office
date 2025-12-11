/** \file bridge_mainloop.h
 *  \ingroup director
 *  \brief Event-driven control bridge loop integrating I/O readiness, frame
 *         decoding (bridge_codec.h) and command dispatch to Director APIs.
 *
 *  Responsibilities
 *  ----------------
 *  - Owns bridge socket(s) and polls for inbound/outbound readiness.
 *  - Performs non-blocking reads, incremental decode, command routing.
 *  - Queues responses / async notifications back to controller client(s).
 *
 *  Concurrency Model
 *  -----------------
 *  Runs on dedicated thread or integrated into Director loop (configuration).
 *  Interacts with Director via lock-free queues or task_queue.h posting.
 *
 *  Error Handling
 *  --------------
 *  Transient socket errors (EAGAIN/EWOULDBLOCK) retried silently; hard
 *  failures close connection and emit an event. Malformed frame -> protocol
 *  error metric increment + optional disconnect.
 *
 *  Security Considerations
 *  -----------------------
 *  Currently assumes trusted local client; future authentication layer may
 *  enforce command authorization.
 */
#ifndef PO_DIRECTOR_BRIDGE_MAINLOOP_H
#define PO_DIRECTOR_BRIDGE_MAINLOOP_H

/* Minimal bridge mainloop API (skeleton).
 * Implementations may choose to run the bridge on a dedicated thread or
 * integrate it into the Director's main loop. These functions provide a
 * small, stable surface for the Director to use while the bridge is fleshed
 * out.
 */

/* Initialize bridge resources. Returns 0 on success. */
int bridge_mainloop_init(void);

/* Run the bridge mainloop. Should block until stopped or an error occurs.
 * Return 0 on clean stop, non-zero on error.
 */
int bridge_mainloop_run(void);

/* Request bridge to stop. Safe to call from signal handlers or other
 * threads; implementation must ensure safe shutdown.
 */
void bridge_mainloop_stop(void);

#endif /* PO_DIRECTOR_BRIDGE_MAINLOOP_H */
