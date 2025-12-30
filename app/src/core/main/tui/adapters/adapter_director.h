/** \file adapter_director.h
 *  \ingroup tui
 *  \brief Adapter converting Director high-level status (tick rate, backlog,
 *         health indicators) into UI consumable metrics and status strings.
 *
 *  Data Sources
 *  ------------
 *  - health_monitor.h snapshot
 *  - scheduler tick stats
 *  - task queue depth & drop counters
 *
 *  Rendering Goals
 *  ---------------
 *  Provide compact, frequently updated summary without allocating per frame.
 *  Employ static buffers / string builder utilities for formatting.
 */

#ifndef ADAPTER_DIRECTOR_H
#define ADAPTER_DIRECTOR_H

#endif // ADAPTER_DIRECTOR_H

