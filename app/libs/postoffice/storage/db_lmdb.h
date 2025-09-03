#ifndef _STORAGE_DB_LMDB_H
#define _STORAGE_DB_LMDB_H

#include <stddef.h>
#include <stdint.h>

#include "lmdb/lmdb.h"
#include "utils/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle for an LMDB environment. */
typedef struct db_env db_env_t;

/** Opaque handle for a named "bucket" (database) within an environment. */
typedef struct db_bucket db_bucket_t;

/**
 * @brief Open or create the on‑disk LMDB environment.
 *
 * @param path           Directory in which to create the data.mdb & lock.mdb files.
 * @param max_databases  Maximum number of named sub‑databases (buckets) you will open.
 * @param map_size       Byte size of the memory map (e.g. 1<<30 for 1 GiB).
 * @param out_env        Receives a pointer to the new environment on success.
 * @return 0 on success, or a negative DB_E* code on failure.
 */
int db_env_open(const char *path, size_t max_databases, size_t map_size, db_env_t **out_env)
    __nonnull((1, 4));

/**
 * @brief Close and free the environment (and all its buckets).
 *
 * @param env  The environment to destroy.
 */
void db_env_close(db_env_t **env) __nonnull((1));

/**
 * @brief Open (or create) a named bucket inside an environment.
 *
 * If the bucket already exists, simply returns its handle.
 *
 * @param env       Opened environment.
 * @param name      Unique ASCII name (must be <= MDB_MAX_DBI_NAME).
 * @param out_bucket Receives the bucket handle.
 * @return 0 on success, or a negative DB_E* code on failure.
 */
int db_bucket_open(db_env_t *env, const char *name, db_bucket_t **out_bucket) __nonnull((1, 2, 3));

/**
 * @brief Close a bucket handle (no-op if not opened).
 *
 * @param bucket  The bucket to close.
 */
void db_bucket_close(db_bucket_t **bucket);

/**
 * @brief Put a key/value pair into the bucket (overwrite if exists).
 *
 * Internally opens a write transaction, does mdb_put, and commits.
 *
 * @param bucket   The bucket handle.
 * @param key      Pointer to key bytes.
 * @param keylen   Length of key in bytes.
 * @param val      Pointer to value bytes.
 * @param vallen   Length of value in bytes.
 * @return 0 on success, DB_E* on error.
 */
int db_put(db_bucket_t *bucket, const void *key, size_t keylen, const void *val, size_t vallen)
    __nonnull((1, 2, 4));

/**
 * @brief Retrieve a value for the given key.
 *
 * Internally opens a read transaction, does mdb_get.  On success, allocates
 * a buffer with malloc() to hold the value; caller must free it.
 *
 * @param bucket    The bucket handle.
 * @param key       Pointer to key bytes.
 * @param keylen    Length of key.
 * @param out_value Receives a malloc()'d pointer to the data.
 * @param out_len   Receives the length of the returned data.
 * @return 0 on success, DB_ENOTFOUND if key not found, or other DB_E* on error.
 */
int db_get(db_bucket_t *bucket, const void *key, size_t keylen, void **out_value, size_t *out_len)
    __nonnull((1, 2, 4, 5));

/**
 * @brief Delete a key/value pair if present.
 *
 * @param bucket  The bucket handle.
 * @param key     Pointer to key bytes.
 * @param keylen  Length of key.
 * @return 0 if deleted, DB_ENOTFOUND if not present, or other DB_E* on error.
 */
int db_delete(db_bucket_t *bucket, const void *key, size_t keylen) __nonnull((1, 2));

/**
 * @brief Iterate all key/value pairs in the bucket in lex order.
 *
 * Opens a read transaction and cursor, then calls `cb(key,keylen,val,vallen,udata)`
 * for each pair.  If `cb` returns non‑zero, iteration stops and that code is
 * returned.  Otherwise returns 0 when complete.
 *
 * @param bucket  The bucket handle.
 * @param cb      Callback invoked for each KV pair.
 * @param udata   User pointer passed through to `cb`.
 * @return 0 on full iteration, or `cb`'s non‑zero value, or DB_E* on error.
 */
int db_iterate(db_bucket_t *bucket,
               int (*cb)(const void *key, size_t keylen, const void *val, size_t vallen,
                         void *udata),
               void *udata) __nonnull((1, 2));

#ifdef __cplusplus
}
#endif

#endif // _STORAGE_DB_LMDB_H
