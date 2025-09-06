/**
 * @file naming.h
 * @brief Project-wide type naming convention documentation and compatibility aliases.
 *
 * Convention (public API types):
 *  - All exported (public) struct / opaque handle / configuration / iterator types
 *    SHOULD use the pattern: `po_<module>_<descriptor>_t` in lowercase snake case.
 *  - All exported POD configuration structs use the same suffix `_t`.
 *  - Legacy names without the `po_` prefix or missing `_t` remain as deprecated
 *    compatibility typedefs for one transition period.
 *  - Enumerators (e.g. LOG_INFO) are left unchanged for readability; an optional
 *    typedef alias with `po_` prefix may be provided for the enum type.
 *
 * Migration status:
 *  - New canonical names: see aliases below. Existing code may continue
 *    compiling; future refactors will replace uses with canonical names.
 */

#ifndef PO_NAMING_H
#define PO_NAMING_H

#ifdef __cplusplus
extern "C" {
#endif

/* GCC / Clang deprecation attribute helper */
#if defined(__GNUC__) || defined(__clang__)
#define PO_DEPRECATED_TYPE(msg) __attribute__((deprecated(msg)))
#else
#define PO_DEPRECATED_TYPE(msg)
#endif

/* Forward includes for original declarations */
#include "log/logger.h"
#include "storage/storage.h"
#include "perf/perf.h"
#include "hashset/hashset.h"
#include "hashtable/hashtable.h"
#include "sysinfo/sysinfo.h"

/* -------------------------------------------------------------------------- */
/* Logger                                                                     */
/* -------------------------------------------------------------------------- */
typedef logger_level_t po_log_level_t; /* enum alias (not deprecated) */
typedef logger_config po_logger_config_t; /* original had no _t suffix */
typedef logger_config po_logger_config PO_DEPRECATED_TYPE("use po_logger_config_t");

/* -------------------------------------------------------------------------- */
/* Storage                                                                    */
/* -------------------------------------------------------------------------- */
typedef storage_config_t po_storage_config_t; /* already had _t, add canonical prefix */
typedef storage_config_t storage_config_t_old PO_DEPRECATED_TYPE("use po_storage_config_t");

/* -------------------------------------------------------------------------- */
/* Perf                                                                       */
/* -------------------------------------------------------------------------- */
typedef perf_counter_t po_perf_counter_t;
typedef perf_timer_t po_perf_timer_t;
typedef perf_histogram_t po_perf_histogram_t;

/* -------------------------------------------------------------------------- */
/* Hash containers                                                            */
/* -------------------------------------------------------------------------- */
typedef hashset_t po_hashset_t;
typedef hashtable_t po_hashtable_t;
typedef hashtable_iter_t po_hashtable_iter_t;

/* -------------------------------------------------------------------------- */
/* Sysinfo                                                                    */
/* -------------------------------------------------------------------------- */
typedef hugepage_info_t po_hugepage_info_t;

#ifdef __cplusplus
}
#endif

#endif /* PO_NAMING_H */
