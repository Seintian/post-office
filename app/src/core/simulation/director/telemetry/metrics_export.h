/** \file metrics_export.h
 *  \ingroup director
 *  \brief Bridges internal perf/metrics subsystem counters to external
 *         representations (text snapshot, structured IPC message, or future
 *         remote scrape protocol).
 *
 *  Export Formats
 *  --------------
 *  - Human-readable table (aligned columns) for TUI diagnostics view.
 *  - Compact key=value lines for log ingestion.
 *  - Binary IPC frame (planned) for remote monitoring tools.
 *
 *  Performance Constraints
 *  -----------------------
 *  Must not stall the Director loop; expensive aggregation executed in a
 *  staged fashion (slice across ticks) or offloaded to a helper thread if
 *  size grows. Snapshot uses stable perf iteration API.
 *
 *  Error Handling
 *  --------------
 *  Allocation failures degrade gracefully by truncating output; caller may
 *  detect partial emission via return code (-1) and skip send.
 *
 *  Future
 *  ------
 *  - Prometheus exposition format.
 *  - Delta-only export for high-frequency streaming.
 *
 *  @see health_monitor.h aggregates some exported counters.
 */
#ifndef PO_DIRECTOR_METRICS_EXPORT_H
#define PO_DIRECTOR_METRICS_EXPORT_H

/* Placeholder for export API (format enum, write function). */

#endif /* PO_DIRECTOR_METRICS_EXPORT_H */
