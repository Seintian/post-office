#ifndef _SYSINFO_H
#define _SYSINFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/**
 * @file sysinfo.h
 * @brief System information utilities for adaptive tuning and diagnostics.
 *
 * This module collects runtime data about the host environment: CPU topology,
 * memory, limits, disk, network, and OS details. Applications can query
 * these values to size thread pools, configure caches, and detect resource
 * availability.
 */

/**
 * @struct po_sysinfo
 * @brief Aggregated system information.
 */
typedef struct {
    /* CPU topology */
    int      physical_cores;      /**< Number of physical CPU cores */
    long     logical_processors;  /**< Number of logical processors (threads) */

    /* Cache sizes (bytes) */
    long     cache_l1;            /**< L1 cache size per core */
    long     dcache_l1;           /**< Data cache size per core */
    long     cache_l2;            /**< L2 cache size per core */
    long     cache_l3;            /**< L3 cache size per NUMA node */

    /* Memory */
    long     total_ram;           /**< Total physical RAM (bytes) */
    long     free_ram;            /**< Free RAM at startup (bytes) */
    long     page_size;           /**< System page size (bytes) */
    size_t   hugepage_size;       /**< Default huge page size (bytes), 0 if unsupported */

    /* Limits */
    long     max_open_files;      /**< RLIMIT_NOFILE soft limit */
    long     max_processes;       /**< RLIMIT_NPROC soft limit */
    long     max_stack_size;      /**< RLIMIT_STACK soft limit (bytes) */

    /* Disk */
    uint64_t disk_free;           /**< Free disk space at data directory (bytes) */
    char     fs_type[16];         /**< Filesystem type (e.g., "ext4", "xfs") */

    /* Network */
    int      mtu;                 /**< MTU of primary network interface */
    int      somaxconn;           /**< /proc/sys/net/core/somaxconn */

    /* OS */
    int      is_little_endian;    /**< 1 if little-endian, 0 if big-endian */
} po_sysinfo_t;

/**
 * @brief Populate a po_sysinfo_t structure with current system data.
 *
 * @param[out] info  Pointer to a `po_sysinfo_t` to fill. Must not be NULL.
 * @return 0 on success, non-zero on failure.
 */
int po_sysinfo_collect(po_sysinfo_t *info);

/**
 * @brief Print collected sysinfo for debugging.
 *
 * Formats and writes the contents of info to the specified FILE stream.
 *
 * @param[in] info  Collected system info. Must not be NULL.
 * @param[in] out   FILE stream to write to (e.g., stdout).
 */
void po_sysinfo_print(const po_sysinfo_t *info, FILE *out);

#ifdef __cplusplus
}
#endif

#endif // _SYSINFO_H
