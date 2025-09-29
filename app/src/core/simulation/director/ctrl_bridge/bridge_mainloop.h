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

/* Placeholder for mainloop init/run/stop functions. */

#endif /* PO_DIRECTOR_BRIDGE_MAINLOOP_H */
