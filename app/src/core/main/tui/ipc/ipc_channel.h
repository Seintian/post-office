/** \file ipc_channel.h
 *  \ingroup tui
 *  \brief UI-side abstraction over Director control bridge connection for
 *         sending commands and receiving async updates (state diffs, metrics).
 *
 *  Responsibilities
 *  ----------------
 *  - Manage non-blocking socket/pipe descriptors used by TUI.
 *  - Buffer outbound commands (throttle / coalesce where feasible).
 *  - Provide poll hooks for read/write readiness integration in UI loop.
 *
 *  Error Handling
 *  --------------
 *  Connection loss triggers automatic reconnection attempts (backoff) and
 *  UI notification banner. Send after disconnect returns -1 (errno=ENOTCONN).
 */

