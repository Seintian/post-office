/** \file backoff.h
 *  \ingroup director
 *  \brief Exponential + jitter backoff helper for retry loops (IPC reconnect,
 *         transient resource acquisition) used inside Director subsystems.
 *
 *  Algorithm
 *  ---------
 *  next = min(base * 2^attempt, max); apply +/- jitter% to avoid thundering
 *  herd on simultaneous failures. Optionally supports decorrelated jitter.
 *
 *  Usage Pattern
 *  -------------
 *  Initialize with bounds, call step() to obtain next sleep duration (ns / ms
 *  depending on configuration), call reset() on success path.
 *
 *  Thread Safety
 *  -------------
 *  Intended for use on a single thread per instance. Shared usage requires
 *  external synchronization.
 *
 *  Error Handling
 *  --------------
 *  Parameter validation during init returns -1 (EINVAL) for inconsistent
 *  bounds (min > max, zero base, etc.). step() always succeeds.
 */
#ifndef PO_DIRECTOR_BACKOFF_H
#define PO_DIRECTOR_BACKOFF_H

/* Placeholder for struct po_backoff and API: init, step, reset. */

#endif /* PO_DIRECTOR_BACKOFF_H */
