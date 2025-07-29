#ifndef PO_ERRORS_H
#define PO_ERRORS_H

#include "lmdb/lmdb.h"


/**
 * @brief Converts an error code to a human-readable string.
 * 
 * This function maps error codes from various subsystems (LMDB, inih, etc.)
 * to their corresponding error messages.
 * 
 * @param err The error code to convert.
 * @return A human-readable string describing the error.
 */
const char *po_strerror(int err);

// Macros

#define PO_EBASE 1000

// - inih

#define INIH_EBASE PO_EBASE
#define INIH_EOK 0
#define INIH_ENOSECTION (INIH_EBASE + 1)
#define INIH_ENOKEY (INIH_EBASE + 2)
#define INIH_ENOVALUE (INIH_EBASE + 3)
#define INIH_EINVAL (INIH_EBASE + 4)
#define INIH_EGENERIC (INIH_EBASE + 5)
#define INIH_ESYNTAX (INIH_EBASE + 6)
#define INIH_EDUPSECTION (INIH_EBASE + 7)
#define INIH_EDUPKEY (INIH_EBASE + 8)
#define INIH_EUNKKEY (INIH_EBASE + 9)
#define INIH_EUNKSECTION (INIH_EBASE + 10)
#define INIH_NOTFOUND (INIH_EBASE + 11)
#define INIH_ENOUSER (INIH_EBASE + 12)
#define INIH_ERANGE (INIH_EBASE + 13)
#define INIH_ETOP INIH_EUNKSECTION

// - libfort 

// no errors

// - db_lmdb | see: http://www.lmdb.tech/doc/group__errors.html

/**
 * @defgroup DB_ERRORS LMDB Error Codes
 * @brief Application-specific error codes mapping LMDB return values to internal codes.
 *
 * These macros define an error code base (`DB_EBASE`) and enumerated values for each
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
 * @{
 */

/// Base offset for LMDB-related errors relative to project error codes.
#define DB_EBASE (PO_EBASE + 100)

/// Success (maps to MDB_SUCCESS / 0)
#define DB_EOK MDB_SUCCESS

/// Key/data pair already exists (`MDB_KEYEXIST`)
#define DB_EKEYEXIST (DB_EBASE + 1)

/// Key/data pair not found (`MDB_NOTFOUND`)
#define DB_ENOTFOUND (DB_EBASE + 2)

/// Requested page not found or invalid page reference (`MDB_PAGE_NOTFOUND`)
#define DB_EPAGENOTFOUND (DB_EBASE + 3)

/// Database file is corrupted (`MDB_CORRUPTED`)
#define DB_ECORRUPTED (DB_EBASE + 4)

/// Update of meta page failed or environment panicked (`MDB_PANIC`)
#define DB_EPANIC (DB_EBASE + 5)

/// Environment version mismatch (`MDB_VERSION_MISMATCH`)
#define DB_EVERSION (DB_EBASE + 6)

/// File descriptor or environment handle is invalid (`MDB_INVALID`)
#define DB_EINVALID (DB_EBASE + 7)

/// Database map is full; size limit reached (`MDB_MAP_FULL`)
#define DB_EMAPFULL (DB_EBASE + 8)

/// Maximum number of named databases reached (`MDB_DBS_FULL`)
#define DB_EDBSFULL (DB_EBASE + 9)

/// Too many reader slots in the environment (`MDB_READERS_FULL`)
#define DB_EREADERSFULL (DB_EBASE + 10)

/// Transaction has too many dirty pages (`MDB_TXN_FULL`)
#define DB_ETXNFULL (DB_EBASE + 11)

/// Cursor stack is full (`MDB_CURSOR_FULL`)
#define DB_ECURSORFULL (DB_EBASE + 12)

/// Page has no more space for new key/data pairs (`MDB_PAGE_FULL`)
#define DB_EPAGEFULL (DB_EBASE + 13)

/// Environment map resized unexpectedly (`MDB_MAP_RESIZED`)
#define DB_EMAPRESIZED (DB_EBASE + 14)

/// Environment is incompatible with requested flags (`MDB_INCOMPATIBLE`)
///
/// Operation and DB incompatible, or DB type changed. This can mean:
///     The operation expects an `MDB_DUPSORT` / `MDB_DUPFIXED` database.
///     Opening a named DB when the unnamed DB has `MDB_DUPSORT` / `MDB_INTEGERKEY`.
///     Accessing a data record as a database, or vice versa.
///     The database was dropped and recreated with different flags.
#define DB_EINCOMP (DB_EBASE + 15)

/// Invalid reuse of reader slot (`MDB_BAD_RSLOT`)
#define DB_EBADRSLOT (DB_EBASE + 16)

/// Transaction must abort and cannot proceed (`MDB_BAD_TXN`)
#define DB_EBADTXN (DB_EBASE + 17)

/// Invalid key/value size or misuse of DUPFIXED (`MDB_BAD_VALSIZE`)
#define DB_EBADVALSIZE (DB_EBASE + 18)

/// Database handle changed or invalid (`MDB_BAD_DBI`)
#define DB_EBADDBI (DB_EBASE + 19)

/// Cursor not initialized before use (application-specific)
#define DB_ECURSORUNINIT (DB_EBASE + 20)

/// Cursor used from a different thread than it was created on (application-specific)
#define DB_ECURSORTHREAD (DB_EBASE + 21)

/// Write attempted in a read-only transaction (`MDB_TXN_READONLY`)
#define DB_ETXNREADONLY (DB_EBASE + 22)

/// Illegal access or permission denied in LMDB operations (application-specific)
#define DB_EILLACC (DB_EBASE + 23)

/// Top-most error value for range checks
#define DB_ETOP DB_EILLACC

/** @} */ // end of DB_ERRORS

// - log_c

// no errors

// - logging

#define LOG_EBASE (PO_EBASE + 200)
#define LOG_EOK 0
#define LOG_EINVAL (LOG_EBASE + 1) // Invalid log level or parameters

// - storage

// - prime

// no errors

// - perf

#define PERF_EBASE (PO_EBASE + 300)
#define PERF_EOK 0
#define PERF_EALREADY (PERF_EBASE + 1) // Already initialized
#define PERF_ENOCOUNTER (PERF_EBASE + 2) // Counter not found
#define PERF_ENOTIMER (PERF_EBASE + 3) // Timer not found
#define PERF_ENOHISTOGRAM (PERF_EBASE + 4) // Histogram not found
#define PERF_EINVAL (PERF_EBASE + 5) // Invalid parameters or state
#define PERF_ENOTINIT (PERF_EBASE + 6) // Performance subsystem not initialized
#define PERF_EBUSY (PERF_EBASE + 7) // Performance subsystem is busy
#define PERF_EAGAIN (PERF_EBASE + 8) // Resource temporarily unavailable
#define PERF_EOVERFLOW (PERF_EBASE + 9) // Overflow in histogram or counter
#define PERF_ETOP PERF_EOVERFLOW // Top-most error value for range checks

// - net

// - hashtable

// no errors

// - hashset

// no errors

// - core

#endif // PO_ERRORS_H
