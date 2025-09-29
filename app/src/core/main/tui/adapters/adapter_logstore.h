/** \file adapter_logstore.h
 *  \ingroup tui
 *  \brief TUI adapter translating logstore internal records into view model
 *         rows (tail view, filtered search) with minimal per-frame overhead.
 *
 *  Responsibilities
 *  ----------------
 *  - Incremental fetch of new log entries since last frame.
 *  - Apply user filters (severity, substring) and truncation for display.
 *  - Provide stable identifiers for selection & scroll position retention.
 *
 *  Performance
 *  -----------
 *  Avoids reformatting unchanged lines; caches rendered width where possible.
 *
 *  Thread Safety
 *  -------------
 *  Accessed on UI thread; obtains snapshot / iterator from logstore via
 *  exported snapshot API (copy or slice) to avoid blocking appenders.
 *
 *  Error Handling
 *  --------------
 *  Snapshot acquisition failure (ENOMEM) degrades gracefully (no update).
 *
 *  Future
 *  ------
 *  - Regex filtering.
 *  - Color theme mapping by severity.
 */

