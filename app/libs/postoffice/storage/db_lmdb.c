#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "db_lmdb.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Internal LMDB ↔ DB_E* mapping */
static int _map_lmdb_error(int rc) {
    switch (rc) {
    case MDB_SUCCESS:
        return DB_EOK;
    case MDB_KEYEXIST:
        return DB_EKEYEXIST;
    case MDB_NOTFOUND:
        return DB_ENOTFOUND;
    case MDB_PAGE_NOTFOUND:
        return DB_EPAGENOTFOUND;
    case MDB_CORRUPTED:
        return DB_ECORRUPTED;
    case MDB_PANIC:
        return DB_EPANIC;
    case MDB_VERSION_MISMATCH:
        return DB_EVERSION;
    case MDB_INVALID:
        return DB_EINVALID;
    case MDB_MAP_FULL:
        return DB_EMAPFULL;
    case MDB_DBS_FULL:
        return DB_EDBSFULL;
    case MDB_READERS_FULL:
        return DB_EREADERSFULL;
    case MDB_TXN_FULL:
        return DB_ETXNFULL;
    case MDB_CURSOR_FULL:
        return DB_ECURSORFULL;
    case MDB_PAGE_FULL:
        return DB_EPAGEFULL;
    case MDB_MAP_RESIZED:
        return DB_EMAPRESIZED;
    case MDB_INCOMPATIBLE:
        return DB_EINCOMP;
    case MDB_BAD_RSLOT:
        return DB_EBADRSLOT;
    case MDB_BAD_TXN:
        return DB_EBADTXN;
    case MDB_BAD_VALSIZE:
        return DB_EBADVALSIZE;
    case MDB_BAD_DBI:
        return DB_EBADDBI;
    default:
        return DB_EINVALID;
    }
}

// Opaque definitions
struct db_env {
    MDB_env *env;
};

struct db_bucket {
    MDB_env *env;
    MDB_dbi dbi;
};

//----------------------------------------------------------------------
// db_env_open
//----------------------------------------------------------------------

int db_env_open(const char *path, size_t max_databases, size_t map_size, db_env_t **out_env) {
    MDB_env *env;
    int rc = mdb_env_create(&env);
    if (rc != MDB_SUCCESS) {
        errno = _map_lmdb_error(rc);
        return -1;
    }

    rc = mdb_env_set_maxreaders(env, 126);
    if (rc != MDB_SUCCESS) {
        mdb_env_close(env);
        errno = _map_lmdb_error(rc);
        return -1;
    }

    rc = mdb_env_set_maxdbs(env, (unsigned)max_databases);
    if (rc != MDB_SUCCESS) {
        mdb_env_close(env);
        errno = _map_lmdb_error(rc);
        return -1;
    }

    rc = mdb_env_set_mapsize(env, map_size);
    if (rc != MDB_SUCCESS) {
        mdb_env_close(env);
        errno = _map_lmdb_error(rc);
        return -1;
    }

    rc = mdb_env_open(env, path, 0, 0664);
    if (rc != MDB_SUCCESS) {
        mdb_env_close(env);
        errno = _map_lmdb_error(rc);
        return -1;
    }

    db_env_t *dbenv = malloc(sizeof(*dbenv));
    if (!dbenv) {
        mdb_env_close(env);
        errno = ENOMEM;
        return -1;
    }
    dbenv->env = env;

    *out_env = dbenv;
    return 0;
}

//----------------------------------------------------------------------
// db_env_close
//----------------------------------------------------------------------

void db_env_close(db_env_t **dbenv) {
    if (!*dbenv)
        return;

    db_env_t *env = *dbenv;
    if (env->env) {
        mdb_env_close(env->env);
    }
    free(env);
    *dbenv = NULL;
}

//----------------------------------------------------------------------
// db_bucket_open
//----------------------------------------------------------------------

int db_bucket_open(db_env_t *dbenv, const char *name, db_bucket_t **out_bucket) {
    MDB_txn *txn;
    int rc = mdb_txn_begin(dbenv->env, NULL, 0, &txn);
    if (rc != MDB_SUCCESS) {
        errno = _map_lmdb_error(rc);
        return -1;
    }

    MDB_dbi dbi;
    rc = mdb_dbi_open(txn, name, MDB_CREATE, &dbi);
    if (rc != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        errno = _map_lmdb_error(rc);
        return -1;
    }

    rc = mdb_txn_commit(txn);
    if (rc != MDB_SUCCESS) {
        errno = _map_lmdb_error(rc);
        return -1;
    }

    db_bucket_t *bucket = malloc(sizeof(*bucket));
    if (!bucket) {
        errno = ENOMEM;
        return -1;
    }
    bucket->env = dbenv->env;
    bucket->dbi = dbi;

    *out_bucket = bucket;
    return 0;
}

//----------------------------------------------------------------------
// db_bucket_close
//----------------------------------------------------------------------

void db_bucket_close(db_bucket_t **bucket) {
    if (!*bucket)
        return;

    // optional: mdb_dbi_close(bucket->env, bucket->dbi);
    free(*bucket);
    *bucket = NULL;
}

//----------------------------------------------------------------------
// db_put
//----------------------------------------------------------------------

int db_put(db_bucket_t *bucket, const void *key, size_t keylen, const void *val, size_t vallen) {
    if (keylen == 0) {
        errno = EINVAL;
        return -1;
    }

    MDB_txn *txn;
    int rc = mdb_txn_begin(bucket->env, NULL, 0, &txn);
    if (rc != MDB_SUCCESS) {
        errno = _map_lmdb_error(rc);
        return -1;
    }

    MDB_val mdb_key = {.mv_size = keylen, .mv_data = (void *)key};
    MDB_val mdb_val = {.mv_size = vallen, .mv_data = (void *)val};

    rc = mdb_put(txn, bucket->dbi, &mdb_key, &mdb_val, 0);
    if (rc != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        errno = _map_lmdb_error(rc);
        return -1;
    }

    rc = mdb_txn_commit(txn);
    if (rc != MDB_SUCCESS) {
        errno = _map_lmdb_error(rc);
        return -1;
    }

    return 0;
}

//----------------------------------------------------------------------
// db_get
//----------------------------------------------------------------------

int db_get(db_bucket_t *bucket, const void *key, size_t keylen, void **out_value, size_t *out_len) {
    if (keylen == 0) {
        errno = EINVAL;
        return -1;
    }

    MDB_txn *txn;
    int rc = mdb_txn_begin(bucket->env, NULL, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS) {
        errno = _map_lmdb_error(rc);
        return -1;
    }

    MDB_val mdb_key = {.mv_size = keylen, .mv_data = (void *)key};
    MDB_val mdb_val;

    rc = mdb_get(txn, bucket->dbi, &mdb_key, &mdb_val);
    if (rc == MDB_NOTFOUND) {
        mdb_txn_abort(txn);
        errno = DB_ENOTFOUND;
        return -1;
    }
    if (rc != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        errno = _map_lmdb_error(rc);
        return -1;
    }

    // copy out
    void *buf = malloc(mdb_val.mv_size);
    if (!buf) {
        mdb_txn_abort(txn);
        errno = ENOMEM;
        return -1;
    }
    memcpy(buf, mdb_val.mv_data, mdb_val.mv_size);
    *out_value = buf;
    *out_len = mdb_val.mv_size;

    mdb_txn_commit(txn);
    return 0;
}

//----------------------------------------------------------------------
// db_delete
//----------------------------------------------------------------------

int db_delete(db_bucket_t *bucket, const void *key, size_t keylen) {
    if (keylen == 0) {
        errno = EINVAL;
        return -1;
    }

    MDB_txn *txn;
    int rc = mdb_txn_begin(bucket->env, NULL, 0, &txn);
    if (rc != MDB_SUCCESS) {
        errno = _map_lmdb_error(rc);
        return -1;
    }

    MDB_val mdb_key = {.mv_size = keylen, .mv_data = (void *)key};
    rc = mdb_del(txn, bucket->dbi, &mdb_key, NULL);
    if (rc == MDB_NOTFOUND) {
        mdb_txn_abort(txn);
        errno = DB_ENOTFOUND;
        return -1;
    }
    if (rc != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        errno = _map_lmdb_error(rc);
        return -1;
    }

    rc = mdb_txn_commit(txn);
    if (rc != MDB_SUCCESS) {
        errno = _map_lmdb_error(rc);
        return -1;
    }
    return 0;
}

//----------------------------------------------------------------------
// db_iterate
//----------------------------------------------------------------------

int db_iterate(db_bucket_t *bucket,
               int (*cb)(const void *key, size_t keylen, const void *val, size_t vallen,
                         void *udata),
               void *udata) {
    MDB_txn *txn;
    MDB_cursor *cursor;
    int rc = mdb_txn_begin(bucket->env, NULL, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS) {
        errno = _map_lmdb_error(rc);
        return -1;
    }

    rc = mdb_cursor_open(txn, bucket->dbi, &cursor);
    if (rc != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        errno = _map_lmdb_error(rc);
        return -1;
    }

    MDB_val key;
    MDB_val val;
    rc = mdb_cursor_get(cursor, &key, &val, MDB_FIRST);
    for (; rc == MDB_SUCCESS; rc = mdb_cursor_get(cursor, &key, &val, MDB_NEXT)) {
        int stop = cb(key.mv_data, key.mv_size, val.mv_data, val.mv_size, udata);
        if (stop) {
            mdb_cursor_close(cursor);
            mdb_txn_abort(txn);
            return stop;
        }
    }

    mdb_cursor_close(cursor);
    mdb_txn_commit(txn);
    if (rc != MDB_NOTFOUND) {
        errno = _map_lmdb_error(rc);
        return -1;
    }
    return 0;
}
