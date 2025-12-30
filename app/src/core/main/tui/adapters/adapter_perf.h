/** \file adapter_perf.h
 *  \ingroup tui
 *  \brief TUI adapter exposing perf/metrics subsystem snapshots (counters,
 *         timers, histograms) to performance dashboard widgets.
 *
 *  Responsibilities
 *  ----------------
 *  - Periodic snapshot of metrics (throttled to avoid excessive copying).
 *  - Derive human-friendly representations (rates, latency percentiles).
 *  - Provide diff vs previous snapshot for sparkline / trend widgets.
 *
 *  Performance Considerations
 *  --------------------------
 *  Heavy computations (percentile extraction) amortized across frames or
 *  cached until underlying histogram version changes.
 *
 *  Thread Safety
 *  -------------
 *  UI thread only; underlying perf iteration API is thread-safe for readers.
 *
 *  Failure Modes
 *  -------------
 *  Snapshot allocation failure -> partial update; widgets display last data.
 *
 *  Future
 *  ------
 *  - Export to file from UI.
 *  - Custom user-defined metric grouping.
 */

#ifndef ADAPTER_PERF_H
#define ADAPTER_PERF_H

#endif // ADAPTER_PERF_H
