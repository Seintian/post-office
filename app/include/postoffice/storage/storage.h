/**
 * @file storage.h
 * @ingroup storage
 * @brief Umbrella include + high-level lifecycle API for the storage
 *        subsystem (append-only log store + LMDB-backed index).
 *
 * This header intentionally exposes only a minimal initialization / shutdown
 * surface and forwards the rich log store API via `#include <postoffice/storage/logstore.h>`.
 * The design aims to keep typical embedding code simple:
 *
 *   1. Fill a ::po_storage_config_t with directory, sizing and batching
 *      parameters (see each field for semantics and constraints).
 *   2. Call ::po_storage_init(). On success, the default log store instance
 *      is created and ready for ::po_logstore_append()/::po_logstore_get().
 *   3. Optionally retrieve the default log store with ::po_storage_logstore().
 *   4. Call ::po_storage_shutdown() at process teardown or on fatal error.
 *
 * Threading / concurrency:
 *  - Initialization is not thread-safe; call once before concurrent use.
 *  - After successful init, the underlying log store implementation provides
 *    internal synchronization for append / get operations (see logstore docs).
 *
 * Error handling:
 *  - ::po_storage_init() returns 0 on success, -1 on failure (errno set from
 *    LMDB (e.g. ENOENT, EACCES, MDB_MAP_FULL), allocation failures (ENOMEM),
 *    or argument validation (EINVAL)). Partial initialization attempts are
 *    rolled back to avoid resource leaks.
 *  - ::po_storage_shutdown() is idempotent; safe to call if not initialized.
 *
 * Configuration validation summary:
 *  - `dir` must be non-NULL, existing or creatable with appropriate perms.
 *  - `bucket` (LMDB database name) must be non-NULL and non-empty.
 *  - `map_size` should be a multiple of the system page size; if 0 a default
 *    internal sizing heuristic is applied.
 *  - `ring_capacity` should preferably be a power of two (forwarded to perf
 *    ring buffer); minimum 2.
 *  - `batch_size` must be >= 1.
 *  - `fsync_policy` must be a valid ::po_logstore_fsync_policy_t enumerant.
 *
 * Durability vs throughput:
 *  - See ::po_logstore_fsync_policy_t for detailed policy semantics. Choosing
 *    aggressive batching with `PO_LS_FSYNC_NONE` maximizes ingestion throughput
 *    at the cost of crash recovery exposure. Interval / every-N policies offer
 *    tunable middle ground.
 *
 * Default instance rationale:
 *  - Many applications require only one log store mapping to a single index
 *    / data file set. Providing a canonical instance avoids boilerplate
 *    plumbing while retaining the option to open additional stores explicitly
 *    (future extension) if multi-tenant usage arises.
 *
 * @see po_storage_config
 * @see po_storage_init
 * @see po_storage_logstore
 * @see po_logstore_fsync_policy_t
 * @see logstore.h Detailed append / get / integrity scan API.
 */

#ifndef POSTOFFICE_STORAGE_H
#define POSTOFFICE_STORAGE_H

/* Public storage API is declared in specific headers; this umbrella file exists
 * to provide a stable include and to anchor Doxygen grouping for storage. */

#include <stdbool.h>
#include <stddef.h>

#include "storage/logstore.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Storage subsystem configuration passed to ::po_storage_init().
 *
 * Fields with value 0 (or false) may trigger internal defaults where noted.
 */
typedef struct po_storage_config {
    const char *dir;                         //!< Base directory for on-disk files (must exist or be creatable)
    const char *bucket;                      //!< LMDB database (bucket) name for index.
    size_t map_size;                         //!< LMDB map size in bytes (0 => internal default sizing heuristic).
    size_t ring_capacity;                    //!< Logstore internal queue capacity (power-of-two recommended).
    size_t batch_size;                       //!< Max records per flush batch (>=1); impacts latency vs throughput.
    po_logstore_fsync_policy_t fsync_policy; //!< Durability policy controlling fsync frequency.
    bool attach_logger_sink;                 //!< If true, install a logger sink writing formatted lines to the store.
} po_storage_config_t;

/**
 * @brief Initialize the storage subsystem.
 *
 * Performs LMDB environment setup, creates (or opens) the bucket index, and
 * instantiates the default append-only log store instance with batching &
 * durability behavior derived from @p cfg. Safe to call exactly once; repeated
 * calls without intervening ::po_storage_shutdown() yield -1 with errno=EALREADY.
 *
 * @param cfg Configuration pointer (must be non-NULL; members validated).
 * @return 0 on success; -1 on error (errno set – see file header for classes).
 */
int po_storage_init(const po_storage_config_t *cfg);

/**
 * @brief Shutdown the storage subsystem and release the default log store.
 *
 * Flushes outstanding batches (subject to fsync policy), closes LMDB handles
 * and frees internal memory. Idempotent: safe to invoke if not initialized.
 */
void po_storage_shutdown(void);

/**
 * @brief Obtain the default log store instance created by ::po_storage_init().
 *
 * @return Pointer to log store handle or NULL if not initialized / init failed.
 */
po_logstore_t *po_storage_logstore(void);

#ifdef __cplusplus
}
#endif

#endif /* POSTOFFICE_STORAGE_H */
