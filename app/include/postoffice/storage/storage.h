/**
 * @file storage.h
 * @brief Storage API (LMDB-backed key-value store and log store).
 * @ingroup storage
 */

#ifndef POSTOFFICE_STORAGE_H
#define POSTOFFICE_STORAGE_H

/* Public storage API is declared in specific headers; this umbrella file exists
 * to provide a stable include and to anchor Doxygen grouping for storage. */

#include <stddef.h>
#include <stdbool.h>

#include "storage/logstore.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Storage system configuration */
typedef struct storage_config {
	const char *dir;        // base directory for on-disk files
	const char *bucket;     // LMDB bucket name for index
	size_t map_size;        // LMDB map size
	size_t ring_capacity;   // logstore queue capacity
	size_t batch_size;      // logstore batch size
	po_logstore_fsync_policy_t fsync_policy; // fsync policy
	bool attach_logger_sink; // attach logger sink to store formatted lines
} storage_config_t;

/** Initialize storage subsystem and create a default logstore instance. */
int storage_init(const storage_config_t *cfg);

/** Shutdown storage subsystem and free resources. */
void storage_shutdown(void);

/** Get the default logstore instance (NULL if not initialized). */
po_logstore_t *storage_logstore(void);

#ifdef __cplusplus
}
#endif

#endif /* POSTOFFICE_STORAGE_H */
