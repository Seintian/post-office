/**
 * @file fsinfo.h
 * @ingroup sysinfo
 * @brief Internal filesystem information helpers used by the sysinfo collector.
 *
 * These helper routines provide small focused queries that populate the
 * `disk_free` and `fs_type` fields of ::po_sysinfo_t. They are intentionally
 * narrow in scope and avoid pulling in wider dependencies so that failure in
 * one probe does not compromise the remainder of system information
 * collection.
 *
 * Design / behavior:
 *  - `free_disk_space()` performs a `statvfs(2)` and returns the number of
 *    free bytes available to unprivileged processes (fragment size * f_bavail).
 *    On error it returns 0 (the caller treats 0 as an absence / sentinel and
 *    may still succeed overall if other probes work).
 *  - `get_fs_type()` scans `/proc/mounts` linearly to find the longest mount
 *    point prefix that matches the provided `path` and copies the filesystem
 *    type (e.g. "ext4", "xfs") into the supplied buffer. It returns 0 on
 *    success, -1 on failure (errno set). A failure results in an empty
 *    filesystem type string in the aggregated snapshot.
 *
 * Concurrency & thread-safety: Functions are reentrant and perform no global
 * mutation. They allocate only stack memory and at most open a FILE* or file
 * descriptor briefly, which is immediately closed.
 *
 * Error handling:
 *  - `free_disk_space()`: returns 0 on failure (errno from `statvfs` may be
 *    inspected by caller if needed).
 *  - `get_fs_type()`: returns -1 on failure and sets errno to the underlying
 *    cause or ENOENT if no matching mount entry was found.
 *
 * Ownership: Callers supply buffers; no allocations are returned that require
 * freeing. These functions are not part of the installed public API (internal
 * header) and may change without notice.
 */
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return number of free bytes on the filesystem containing `path`.
 *
 * Uses `statvfs`. On error returns 0. This matches the semantics in
 * ::po_sysinfo_collect where a 0 value is treated as an unknown / probe
 * failure (not necessarily that the disk is completely full).
 */
unsigned long free_disk_space(const char *path);

/**
 * @brief Determine filesystem type string for the mount containing `path`.
 *
 * Performs a linear scan of `/proc/mounts`. On success writes a NUL-terminated
 * string (possibly truncated) into `fs_type` and returns 0. On failure returns
 * -1 and sets errno (ENOENT if no mount matched).
 *
 * @param path    Path whose containing mount should be resolved.
 * @param fs_type Output buffer for filesystem type.
 * @param size    Size of `fs_type` buffer.
 */
int get_fs_type(const char *path, char *fs_type, size_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif
