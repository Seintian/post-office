/** \file ipc_decode.h
 *  \ingroup tui
 *  \brief Incremental decoder for Director->UI streamed messages (framed
 *         control / telemetry). Consumes raw bytes and invokes callbacks.
 *
 *  Design
 *  ------
 *  - Maintains partial header state; once complete, allocates/points to
 *    payload buffer then emits message when fully received.
 *  - Validates length & version fields; rejects oversized frames.
 *
 *  Error Handling
 *  --------------
 *  Malformed frame returns -1 (errno=EPROTO) and drops current buffer.
 */

