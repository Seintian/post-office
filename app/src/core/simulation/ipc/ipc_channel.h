/** \file ipc_channel.h
 *  \ingroup director
 *  \brief Abstraction over low-level pipe / socket pairs used for reliable
 *         point-to-point IPC between Director and simulation processes.
 *
 *  Features
 *  --------
 *  - Non-blocking I/O with edge-trigger friendly buffering.
 *  - Framed message boundaries (delegates to ipc_codec.h for encode/decode).
 *  - Backpressure signaling via send queue watermarks.
 *
 *  Concurrency
 *  -----------
 *  Each channel typically owned by one thread per endpoint; cross-thread
 *  writes require external synchronization. Internal state (buffers, cursors)
 *  not thread-safe.
 *
 *  Error Handling
 *  --------------
 *  - send: -1 on EAGAIN (queue full) or fatal errors (errno set).
 *  - recv: -1 on EAGAIN, 0 on orderly close, -1 with errno on failure.
 *
 *  Performance Notes
 *  -----------------
 *  Aggregates small writes where possible; consider zero-copy integration
 *  for large payloads in future revisions.
 *
 *  Security
 *  --------
 *  Assumes trusted local processes. Input validation done at codec layer.
 */
#ifndef PO_DIRECTOR_IPC_CHANNEL_H
#define PO_DIRECTOR_IPC_CHANNEL_H

/* Placeholder for channel struct & API. */

#endif /* PO_DIRECTOR_IPC_CHANNEL_H */
