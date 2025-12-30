/** \file adapter_entities.h
 *  \ingroup tui
 *  \brief Adapter flattening entity state_store.h snapshots into table rows
 *         with stable ordering and derived display attributes.
 *
 *  Features
 *  --------
 *  - Column projection (ID, role, state, uptime, error counters).
 *  - Optional filter (role/state) with incremental re-evaluation.
 *  - Stable row identity for selection persistence.
 *
 *  Performance
 *  -----------
 *  Minimizes per-frame allocations by reusing a vector of row structs and
 *  marking dirty rows when underlying entity changed.
 */

#ifndef ADAPTER_ENTITIES_H
#define ADAPTER_ENTITIES_H

#endif // ADAPTER_ENTITIES_H
