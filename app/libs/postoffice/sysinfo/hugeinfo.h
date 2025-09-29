/**
 * @file hugeinfo.h
 * @ingroup sysinfo
 * @brief Helper routines for enumerating and sampling Linux huge page state.
 *
 * The sysinfo collector performs a lightweight snapshot of huge page
 * provisioning so that consumers can reason about large page availability and
 * tune memory allocation strategies. The helpers here isolate direct sysfs
 * traversal from higher-level aggregation logic.
 *
 * Data sources:
 *  - `/sys/kernel/mm/hugepages/hugepages-*kB/` directories for each supported
 *    huge page size. Filenames read: `nr_hugepages`, `free_hugepages`,
 *    `nr_overcommit_hugepages`, `surplus_hugepages`, `resv_hugepages`.
 *
 * Functions:
 *  - `list_hugepage_sizes()`: Enumerates discovered huge page size directory
 *    names, parsing the numeric size in KiB. Best-effort—returns as many as
 *    fit in the provided array.
 *  - `get_hugepage_info()`: Populates a ::po_hugepage_info_t for a specific
 *    size. All reads must succeed or the call fails; partial data is not
 *    committed.
 *
 * Concurrency: Reentrant; no global state mutated. Each read opens and closes
 * a file descriptor immediately.
 *
 * Error handling:
 *  - Both functions return 0 on success, -1 on failure. `errno` is set to the
 *    first failing syscall / parsing issue (e.g., ENOENT if sysfs entries are
 *    absent on kernels without huge page support).
 *
 * Ownership: Callers provide storage; no heap memory ownership is transferred.
 *
 * Stability: Internal helper API not guaranteed stable across releases.
 */
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>

#include "sysinfo/sysinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumerate available huge page sizes (in KiB).
 *
 * @param sizes_kB Output array to receive sizes.
 * @param max      Capacity of @p sizes_kB.
 * @param count    Output number of sizes written.
 * @return 0 on success; -1 on failure (errno set).
 */
int list_hugepage_sizes(unsigned long *sizes_kB, size_t max, size_t *count);

/**
 * @brief Populate huge page statistics for the given page size (KiB).
 *
 * All required sysfs files must be readable; otherwise returns -1 and leaves
 * @p info content unspecified.
 *
 * @param size_kB Huge page size in KiB (e.g. 2048 for 2 MiB).
 * @param info    Output structure pointer.
 * @return 0 on success; -1 on error (errno set).
 */
int get_hugepage_info(unsigned long size_kB, po_hugepage_info_t *info);

#ifdef __cplusplus
} /* extern "C" */
#endif
