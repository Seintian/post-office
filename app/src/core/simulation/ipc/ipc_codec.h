/** \file ipc_codec.h
 *  \ingroup director
 *  \brief Length-prefixed message codec for simulation intra-process control
 *         messages (entity commands, status updates, telemetry bursts).
 *
 *  Frame Layout
 *  ------------
 *  | 2B version | 1B type | 1B flags | 4B length | payload ... |
 *  Mirrors network protocol but optimized for local IPC (endianness assumed
 *  native; conversion only if cross-arch planned).
 *
 *  API (planned)
 *  -------------
 *  - encode(msg, buf, cap) -> size / -1
 *  - decode(stream_state, bytes_in, callback)
 *
 *  Error Handling
 *  --------------
 *  - Invalid header fields -> -1 (errno=EPROTO).
 *  - Oversized length -> -1 (errno=EMSGSIZE).
 *
 *  Streaming Decode
 *  ----------------
 *  Maintains partial state across invocations enabling incremental fills.
 */
#ifndef PO_DIRECTOR_IPC_CODEC_H
#define PO_DIRECTOR_IPC_CODEC_H

/* Placeholder for codec state & functions. */

#endif /* PO_DIRECTOR_IPC_CODEC_H */
