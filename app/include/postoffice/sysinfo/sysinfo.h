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
 * Thread-safety and reentrancy:
 *  - `po_sysinfo_collect` is safe to call concurrently from multiple threads
 *    provided each caller supplies a distinct `po_sysinfo_t *info` output
 *    buffer. The function writes into the caller-supplied `info` structure and
 *    does not serialize access to that buffer. Callers must avoid sharing the
 *    same output struct across threads without external synchronization.
 *  - The collector uses only local variables and best-effort reads from
 *    kernel interfaces; it does not rely on global mutable state and therefore
 *    does not conflict with the background sampler except for reading the same
 *    kernel-provided statistics.
 *
 * Error vs "unavailable" behaviour:
 *  - Returns 0 on overall success. Returns -1 on a hard failure (for
 *    example when a required syscall fails or when `info` is NULL); in hard
 *    failure cases `errno` is set where applicable.
 *  - Several sub-collectors perform best-effort sampling: they may choose to
 *    return success while setting specific fields to sentinel values (e.g.
 *    `cpu_util_pct == -1.0`) when transient sampling fails. In that case the
 *    function typically returns 0 but the field values indicate "unavailable".
 *  - Example: short-interval CPU sampling will set `cpu_util_pct` and
 *    `cpu_iowait_pct` to -1.0 if sampling failed, while other fields may still
 *    be populated. If reading `/proc/meminfo` fails the collector will return
 *    -1 and `errno` is set.
 *
 * NULL pointer handling:
 *  - `info` must be non-NULL; passing NULL yields -1 and `errno` is set to
 *    `EINVAL`.
 *
 * @param[out] info Output pointer (must be non-NULL).
 * @return 0 on success; -1 on hard failure (errno reflects the cause when set).
 * @note Thread-safe: Yes (If info is unique per thread).
 */
int po_sysinfo_collect(po_sysinfo_t *info);

/**
 * @brief Initialize background system info sampler (optional).
 *
 * Starts a background thread that periodically samples dynamic system
 * parameters (CPU utilization, I/O wait percentage) and stores them in an
 * internal cache for callers that prefer lightweight reads via
 * ::po_sysinfo_sampler_get(). The sampler is an optional performance
 * optimization and is not required for ::po_sysinfo_collect().
 *
 * Idempotency and concurrency:
 *  - Calling `po_sysinfo_sampler_init()` when the sampler is already running
 *    will return 0 and not create a second sampler thread in the common
 *    (single-threaded) usage pattern. However, the implementation does not
 *    serialize concurrent calls to `po_sysinfo_sampler_init()`; if two
 *    threads concurrently call init there is a race that may result in
 *    multiple sampler threads being created. Therefore callers MUST ensure
 *    that start/stop are coordinated by the application (for example, only a
 *    single initialization path should call init/stop).
 *
 * Resource cleanup:
 *  - `po_sysinfo_sampler_stop()` joins the sampler thread and clears cached
 *    values. Callers should invoke `po_sysinfo_sampler_stop()` during orderly
 *    shutdown to ensure the sampler thread is cleanly terminated.
 *  - If the application exits without calling `po_sysinfo_sampler_stop()`,
 *    process termination will stop all threads and the OS will reclaim
 *    resources; this is less graceful and may bypass internal cleanup logic.
 *
 * Return and errno semantics:
 *  - Returns 0 on success. On thread-creation failure the function returns
 *    non-zero (typically -1). The pthread API error code is returned from
 *    pthread_create internally; callers should treat any non-zero return as
 *    an error and should not rely on `errno` being set by this call.
 *
 * @return 0 on success, non-zero on failure.
 * @note Thread-safe: No (Concurrent init races).
 */
int po_sysinfo_sampler_init(void);

/**
 * @brief Stop background system info sampler.
 *
 * Idempotency:
 *  - `po_sysinfo_sampler_stop()` is safe to call multiple times; a second
 *    call after the sampler has stopped is a no-op.
 *  - As with init, callers must coordinate start/stop invocations to avoid
 *    races when multiple threads may attempt to start or stop the sampler.
 * @note Thread-safe: No (Concurrent stop races).
 */
void po_sysinfo_sampler_stop(void);

/**
 * @brief Get the latest sampled CPU utilization and I/O wait percentages (0..100).
 *
 * Behavior and semantics:
 *  - If the sampler thread is running, the function copies internally cached
 *    values into the provided output locations and returns 0.
 *  - If the sampler is not running the function returns -1 to indicate the
 *    sampler is unavailable. In that case cached values are not copied.
 *  - If the sampler is running but the sampled data is currently
 *    unavailable, the function will set the output values to -1.0 and return
 *    0 to indicate success with "unavailable" data.
 *
 * Thread-safety:
 *  - `po_sysinfo_sampler_get()` may be called concurrently from multiple
 *    threads. The implementation protects access to the internal cached values
 *    with a mutex so concurrent readers get a consistent snapshot.
 *  - It is safe to call `po_sysinfo_sampler_get()` concurrently with
 *    `po_sysinfo_collect()`; the sampler and collector operate on distinct
 *    memory and kernel interfaces and do not conflict.
 *
 * NULL pointer handling:
 *  - Either `cpu_util_pct` or `cpu_iowait_pct` (or both) may be NULL. Passing
 *    NULL for an output parameter indicates the caller does not require that
 *    specific value.
 *
 * Return values:
 *  - 0 on success (values copied; copied values may be -1.0 to indicate
 *    "unavailable").
 *  - -1 if the sampler is not running or if a hard error occurs.
 * 
 * @param[out] cpu_util_pct Output pointer for CPU utilization (nullable).
 * @param[out] cpu_iowait_pct Output pointer for I/O wait (nullable).
 * @return 0 on success, -1 if sampler not running/error.
 * @note Thread-safe: Yes.
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
