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

typedef struct {
    unsigned long size_kB;  // page size in kB - more useful than it would in bytes
    long nr;                // nr_hugepages
    long free;              // free_hugepages
    long overcommit;        // nr_overcommit_hugepages
    long surplus;           // surplus_hugepages
    long reserved;          // resv_hugepages
} hugepage_info_t;

/**
 * @struct po_sysinfo
 * @brief Aggregated system information.
 */
typedef struct {
    /* CPU topology */
    int32_t         physical_cores;         /**< Number of physical CPU cores */
    int64_t         logical_processors;     /**< Number of logical processors (threads) */

    /* Cache sizes (bytes) */
    int64_t         cache_l1;               /**< L1 cache size per core */
    int64_t         dcache_lnsize;          /**< Data cache line size (bytes) */
    int64_t         dcache_l1;              /**< Data cache size per core */
    int64_t         cache_l2;               /**< L2 cache size per core */
    int64_t         cache_l3;               /**< L3 cache size per NUMA node */

    /* Memory */
    int64_t         total_ram;              /**< Total physical RAM (bytes) */
    int64_t         free_ram;               /**< Free RAM at startup (bytes) */
    int64_t         page_size;              /**< System page size (bytes) */
    hugepage_info_t hugepage_info;          /**< Huge page information */

    /* Limits */
    uint64_t        max_open_files;         /**< RLIMIT_NOFILE soft limit */
    uint64_t        max_processes;          /**< RLIMIT_NPROC soft limit */
    uint64_t        max_stack_size;         /**< RLIMIT_STACK soft limit (bytes) */

    /* Disk */
    uint64_t        disk_free;              /**< Free disk space at data directory (bytes) */
    char            fs_type[16];            /**< Filesystem type (e.g., "ext4", "xfs") */

    /* Network */
    int32_t         mtu;                    /**< MTU of primary network interface */
    int32_t         somaxconn;              /**< /proc/sys/net/core/somaxconn */

    /* OS */
    int32_t         is_little_endian;       /**< 1 if little-endian, 0 if big-endian */
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
