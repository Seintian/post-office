#ifndef _SYSINFO_H
#define _SYSINFO_H

/**
 * @file sysinfo.h
 * @ingroup sysinfo
 * @brief System capability & resource snapshot utilities.
 *
 * Provides a single aggregation API (::po_sysinfo_collect) that queries a
 * variety of platform characteristics (CPU topology, caches, memory, hugepage
 * provisioning, process limits, networking parameters, filesystem stats) and
 * stores them into a compact ::po_sysinfo_t record for later inspection or
 * diagnostic output through ::po_sysinfo_print().
 *
 * Typical usage:
 * @code
 *   po_sysinfo_t info;
 *   if (po_sysinfo_collect(&info) == 0) {
 *       po_sysinfo_print(&info, stdout);
 *   } else {
 *       perror("po_sysinfo_collect");
 *   }
 * @endcode
 *
 * Error handling: ::po_sysinfo_collect returns 0 on full success, -1 on a
 * hard failure (errno set). Some fields may still have been populated on
 * failure—callers requiring all-or-nothing semantics should zero the struct
 * before calling and discard on non-zero return.
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @brief Huge page provisioning snapshot (values from /proc/meminfo or sysfs).
 */
typedef struct {
    unsigned long size_kB; //!< Huge page size in KiB (usually 2048 for 2 MiB pages).
    long nr;               //!< Total huge pages configured.
    long free;             //!< Currently free huge pages.
    long overcommit;       //!< Overcommit allowance (if exposed) else 0.
    long surplus;          //!< Surplus huge pages beyond the static pool.
    long reserved;         //!< Reserved huge pages not available for allocation.
} po_hugepage_info_t;

/**
 * @brief Aggregated system information structure.
 *
 * Numeric fields use signed / unsigned widths sufficient for modern large
 * systems. Semantics:
 *  - Cache sizes in bytes; -1 if not detected.
 *  - RAM values in bytes.
 *  - Limits are soft limit values (resource RLIMIT_* soft) where applicable.
 *  - `is_little_endian` is 1 for little-endian hosts, 0 for big-endian.
 */
typedef struct {
    int32_t physical_cores;     //!< Physical core count (packages * cores per package) or -1.
    int64_t logical_processors; //!< Logical processor (hardware thread) count or -1.
    int64_t cache_l1;           //!< Unified L1 cache size (bytes) or -1.
    int64_t dcache_lnsize;      //!< Data cache line size (bytes) or -1.
    int64_t dcache_l1;          //!< L1 data cache size (bytes) or -1.
    int64_t cache_l2;           //!< L2 cache size (bytes) or -1.
    int64_t cache_l3;           //!< L3 cache size (bytes) or -1.
    int64_t total_ram;          //!< Total system RAM (bytes) or -1.
    int64_t free_ram;           //!< Available RAM (bytes) or -1.
    int64_t swap_total;         //!< Total swap space (bytes) or -1.
    int64_t swap_free;          //!< Free swap space (bytes) or -1.
    int64_t page_size;          //!< Base page size (bytes).
    po_hugepage_info_t hugepage_info; //!< Huge page snapshot.
    uint64_t max_open_files;    //!< RLIMIT_NOFILE soft limit.
    uint64_t max_processes;     //!< RLIMIT_NPROC soft limit (0 if unlimited / unsupported).
    uint64_t max_stack_size;    //!< RLIMIT_STACK soft limit (bytes) or RLIM_INFINITY encoded.
    uint64_t disk_free;         //!< Free bytes on filesystem containing current working dir.
    uint64_t uptime_seconds;    //!< System uptime in seconds since boot.
    double load_avg_1min;       //!< System load average over 1 minute or -1.0 if unavailable.
    double load_avg_5min;       //!< System load average over 5 minutes or -1.0 if unavailable.
    double load_avg_15min;      //!< System load average over 15 minutes or -1.0 if unavailable.
    char fs_type[16];           //!< Filesystem type (truncated) e.g. "ext4", "xfs".
    char hostname[256];         //!< Hostname or empty string if unavailable.
    char cpu_vendor[64];        //!< CPU vendor/manufacturer (e.g., "GenuineIntel", "AuthenticAMD").
    char cpu_brand[256];        //!< CPU brand string or empty string if unavailable.
    double cpu_iowait_pct;      //!< CPU I/O-wait percentage measured over short interval, -1.0 if unavailable.
    double cpu_util_pct;        //!< CPU utilization percentage (active time) over short interval, -1.0 if unavailable.
    int32_t mtu;                //!< Primary interface MTU or -1 if not determined.
    int32_t somaxconn;          //!< net.core.somaxconn sysctl value or -1.
    int32_t is_little_endian;   //!< 1 if little-endian, 0 if big-endian.
} po_sysinfo_t;

/**
 * @brief Collect a best-effort snapshot of system properties into @p info.
 *
 * Sources consulted (best-effort): /proc/cpuinfo, sysconf, sysfs cache
 * hierarchy, /proc/meminfo, getrlimit, statvfs, primary interface ioctl
 * (SIOCGIFMTU), /proc/sys/net/core/somaxconn, endianness test. Unavailable
 * data points are set to sentinel values (-1, 0, empty string).
 *
 * @param info Output pointer (must be non-NULL).
 * @return 0 on success; -1 on failure (errno reflects first hard failure).
 */
int po_sysinfo_collect(po_sysinfo_t *info);

/**
 * @brief Initialize background system info sampler (optional).
 *
 * Starts a background thread that periodically samples dynamic system
 * parameters (CPU utilization, I/O wait percentage) for more accurate
 * readings when ::po_sysinfo_collect is called.
 *
 * @return 0 on success, non-zero on failure.
 */
int po_sysinfo_sampler_init(void);

/**
 * @brief Stop background system info sampler.
 */
void po_sysinfo_sampler_stop(void);

/**
 * @brief Get the latest sampled CPU utilization and I/O wait percentages (0..100).
 *
 * If the sampler is not running or data is unavailable, values are set to -1.0.
 *
 * @param cpu_util_pct Output pointer for CPU utilization percentage.
 * @param cpu_iowait_pct Output pointer for CPU I/O wait percentage.
 * @return 0 on success, non-zero on failure.
 */
int po_sysinfo_sampler_get(double *cpu_util_pct, double *cpu_iowait_pct);

/**
 * @brief Pretty-print collected system information to a FILE*.
 *
 * Formatting is for human inspection and may evolve; not suitable as a stable
 * machine-parse interface.
 *
 * @param info Populated info struct (non-NULL).
 * @param out  Destination stream (non-NULL, writable).
 */
void po_sysinfo_print(const po_sysinfo_t *info, FILE *out);

#ifdef __cplusplus
}
#endif

#endif // _SYSINFO_H
