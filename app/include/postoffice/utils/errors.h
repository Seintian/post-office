#ifndef PO_ERRORS_H
#define PO_ERRORS_H

#include "lmdb/lmdb.h"

const char *po_strerror(int err);

// Macros

#define PO_EBASE 1000

// - inih

#define INIH_EBASE PO_EBASE
#define INIH_EOK 0
#define INIH_ENOSECTION (INIH_EBASE + 1)
#define INIH_ENOKEY (INIH_EBASE + 2)
#define INIH_ENOVALUE (INIH_EBASE + 3)
#define INIH_EINVALIDVALUE (INIH_EBASE + 4)
#define INIH_EGENERIC (INIH_EBASE + 5)
#define INIH_ESYNTAX (INIH_EBASE + 6)
#define INIH_EDUPSECTION (INIH_EBASE + 7)
#define INIH_EDUPKEY (INIH_EBASE + 8)
#define INIH_EUNKKEY (INIH_EBASE + 9)
#define INIH_EUNKSECTION (INIH_EBASE + 10)
#define INIH_NOTFOUND (INIH_EBASE + 11)
#define INIH_ETOP INIH_EUNKSECTION

// - libfort 

// no errors

// - lmdb | see: http://www.lmdb.tech/doc/group__errors.html

/**
 * @defgroup LMDB_ERRORS LMDB Error Codes
 * @brief Application-specific error codes mapping LMDB return values to internal codes.
 *
 * These macros define an error code base (`LMDB_EBASE`) and enumerated values for each
 * LMDB error condition, offset from a project-wide base (`PO_EBASE`). They allow consistent
 * error handling in the application without directly depending on LMDB's negative return values.
 *
 * See also:
 *  - LMDB official error list: http://www.lmdb.tech/doc/group__errors.html
 *  - LMDB conditions (Common Lisp API doc): https://github.com/antimer/lmdb#x-28LMDB-3A-40LMDB-2FERROR-CODE-CONDITIONS-20MGL-PAX-3ASECTION-29
 *
 * Each error corresponds to a documented LMDB return value and can be converted to a
 * human-readable string with `mdb_strerror(int)` if the raw LMDB error code is known.
 *
 * Usage example:
 * @code
 * int rc = mdb_put(txn, dbi, &key, &data, 0);
 * if (rc != MDB_SUCCESS) {
 *     errno = LMDB_EBASE + (abs(rc) - abs(MDB_KEYEXIST)); // example mapping
 *     return -1;
 * }
 * @endcode
 *
 * @{
 */

/// Base offset for LMDB-related errors relative to project error codes.
#define LMDB_EBASE (PO_EBASE + 100)

/// Success (maps to MDB_SUCCESS / 0)
#define LMDB_EOK MDB_SUCCESS

/// Key/data pair already exists (`MDB_KEYEXIST`)
#define LMDB_EKEYEXIST (LMDB_EBASE + 1)

/// Key/data pair not found (`MDB_NOTFOUND`)
#define LMDB_ENOTFOUND (LMDB_EBASE + 2)

/// Requested page not found or invalid page reference (`MDB_PAGE_NOTFOUND`)
#define LMDB_EPAGENOTFOUND (LMDB_EBASE + 3)

/// Database file is corrupted (`MDB_CORRUPTED`)
#define LMDB_ECORRUPTED (LMDB_EBASE + 4)

/// Update of meta page failed or environment panicked (`MDB_PANIC`)
#define LMDB_EPANIC (LMDB_EBASE + 5)

/// Environment version mismatch (`MDB_VERSION_MISMATCH`)
#define LMDB_EVERSION (LMDB_EBASE + 6)

/// File descriptor or environment handle is invalid (`MDB_INVALID`)
#define LMDB_EINVALID (LMDB_EBASE + 7)

/// Database map is full; size limit reached (`MDB_MAP_FULL`)
#define LMDB_EMAPFULL (LMDB_EBASE + 8)

/// Maximum number of named databases reached (`MDB_DBS_FULL`)
#define LMDB_EDBSFULL (LMDB_EBASE + 9)

/// Too many reader slots in the environment (`MDB_READERS_FULL`)
#define LMDB_EREADERSFULL (LMDB_EBASE + 10)

/// Transaction has too many dirty pages (`MDB_TXN_FULL`)
#define LMDB_ETXNFULL (LMDB_EBASE + 11)

/// Cursor stack is full (`MDB_CURSOR_FULL`)
#define LMDB_ECURSORFULL (LMDB_EBASE + 12)

/// Page has no more space for new key/data pairs (`MDB_PAGE_FULL`)
#define LMDB_EPAGEFULL (LMDB_EBASE + 13)

/// Environment map resized unexpectedly (`MDB_MAP_RESIZED`)
#define LMDB_EMAPRESIZED (LMDB_EBASE + 14)

/// Environment is incompatible with requested flags (`MDB_INCOMPATIBLE`)
///
/// Operation and DB incompatible, or DB type changed. This can mean:
///     The operation expects an `MDB_DUPSORT` / `MDB_DUPFIXED` database.
///     Opening a named DB when the unnamed DB has `MDB_DUPSORT` / `MDB_INTEGERKEY`.
///     Accessing a data record as a database, or vice versa.
///     The database was dropped and recreated with different flags.
#define LMDB_EINCOMPATIBLE (LMDB_EBASE + 15)

/// Invalid reuse of reader slot (`MDB_BAD_RSLOT`)
#define LMDB_EBADRSLOT (LMDB_EBASE + 16)

/// Transaction must abort and cannot proceed (`MDB_BAD_TXN`)
#define LMDB_EBADTXN (LMDB_EBASE + 17)

/// Invalid key/value size or misuse of DUPFIXED (`MDB_BAD_VALSIZE`)
#define LMDB_EBADVALSIZE (LMDB_EBASE + 18)

/// Database handle changed or invalid (`MDB_BAD_DBI`)
#define LMDB_EBADDBI (LMDB_EBASE + 19)

/// Cursor not initialized before use (application-specific)
#define LMDB_ECURSORUNINIT (LMDB_EBASE + 20)

/// Cursor used from a different thread than it was created on (application-specific)
#define LMDB_ECURSORTHREAD (LMDB_EBASE + 21)

/// Write attempted in a read-only transaction (`MDB_TXN_READONLY`)
#define LMDB_ETXNREADONLY (LMDB_EBASE + 22)

/// Illegal access or permission denied in LMDB operations (application-specific)
#define LMDB_EILLACCESS (LMDB_EBASE + 23)

/// Top-most error value for range checks
#define LMDB_ETOP LMDB_EILLACCESS

/** @} */ // end of LMDB_ERRORS

// - storage

// - prime

// no errors

// - perf

// - net

// - hashtable

// - core

#endif // PO_ERRORS_H
