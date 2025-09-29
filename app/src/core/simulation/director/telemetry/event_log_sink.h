/** \file event_log_sink.h
 *  \ingroup director
 *  \brief Structured append-only event sink capturing state transitions and
 *         notable simulation incidents for diagnostics & UI timelines.
 *
 *  Event Model
 *  -----------
 *  - Fixed schema (timestamp, category, entity id, prev_state, new_state,
 *    detail string / code) enabling stable parsing.
 *  - Append-only ring / log buffer with truncation policy (drop oldest) to
 *    preserve recent history without unbounded growth.
 *
 *  Concurrency
 *  -----------
 *  Single writer (Director thread) appends; multiple readers (UI, exporter)
 *  iterate over a snapshot or copy. Memory barrier after write publishes
 *  entry before head index update.
 *
 *  Error Handling
 *  --------------
 *  Allocation failures at init -> errno=ENOMEM. Append after failure returns
 *  -1 to allow caller to decide (best effort). Truncation is silent.
 *
 *  Future Enhancements
 *  -------------------
 *  - Binary export / compression for large replay traces.
 *  - Subscription filtering (category-based consumer cursors).
 *
 *  @see health_monitor.h for derived health metrics.
 */
#ifndef PO_DIRECTOR_EVENT_LOG_SINK_H
#define PO_DIRECTOR_EVENT_LOG_SINK_H

/* Placeholder for event struct & append/query APIs. */

#endif /* PO_DIRECTOR_EVENT_LOG_SINK_H */
