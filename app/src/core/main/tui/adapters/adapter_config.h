/** \file adapter_config.h
 *  \ingroup tui
 *  \brief Adapter exposing immutable & runtime-adjustable configuration
 *         parameters to the UI (read-only panels + potential future edits).
 *
 *  Responsibilities
 *  ----------------
 *  - Collect structured snapshot of Director/runtime configuration fields.
 *  - Normalize units (bytes -> human readable, durations -> ms/ns strings).
 *  - Provide diff vs previous snapshot to highlight changed keys.
 *
 *  Thread Safety
 *  -------------
 *  Invoked on UI thread; obtains snapshot via configuration API returning a
 *  const struct reference or copied struct (no mutation here).
 *
 *  Future
 *  ------
 *  - Live edit (write-back) with validation highlighting.
 *  - Export to file command from UI.
 */

#ifndef ADAPTER_CONFIG_H
#define ADAPTER_CONFIG_H

#endif // ADAPTER_CONFIG_H
