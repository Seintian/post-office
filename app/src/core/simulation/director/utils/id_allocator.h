/** \file id_allocator.h
 *  \ingroup director
 *  \brief Monotonic (optionally recyclable) small integer ID allocator for
 *         processes / entities managed by the Director.
 *
 *  Modes
 *  -----
 *  - Monotonic: simple ++ counter (fast path, never reuses IDs until wrap).
 *  - Free list: maintains stack of returned IDs to bound max in-flight range.
 *
 *  Wrap Strategy
 *  -------------
 *  64-bit counters make wrap practically unreachable; if 32-bit mode is
 *  configured, reaching max triggers either an error (errno=ERANGE) or a scan
 *  for recyclable IDs (configuration dependent).
 *
 *  Concurrency
 *  -----------
 *  Allocations performed on Director thread; if cross-thread allocation is
 *  introduced, use atomic fetch-add for monotonic mode and a lock for free
 *  list mode (rare path).
 *
 *  Error Handling
 *  --------------
 *  - init: ENOMEM for free list backing storage.
 *  - allocate: ERANGE on exhaustion (non-recoverable without recycling).
 *  - free: EINVAL if ID outside issued range or already freed (debug mode).
 */
#ifndef PO_DIRECTOR_ID_ALLOCATOR_H
#define PO_DIRECTOR_ID_ALLOCATOR_H

/* Placeholder for allocator struct & API. */

#endif /* PO_DIRECTOR_ID_ALLOCATOR_H */
