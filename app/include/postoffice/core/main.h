/**
 * @file main.h
 * @ingroup executables
 * @brief Primary entry orchestrating initialization sequence and command-line integration.
 *
 * Sequence
 * --------
 * 1. Parse arguments / load configs.
 * 2. Initialize logger → perf/metrics → storage → networking primitives.
 * 3. Launch director (or inline run) and subordinate processes.
 * 4. Enter top-level wait / signal loop.
 *
 * Provides declarations (if any) for helpers invoked by the canonical `main()` in executables.
 */

#ifndef PO_CORE_MAIN_H
#define PO_CORE_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PO_CORE_MAIN_H */
