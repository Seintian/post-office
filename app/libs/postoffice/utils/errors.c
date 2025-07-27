#include "utils/errors.h"
#include <string.h>


#define IN(x, base, top) ((x) >= (base) && (x) <= (top))

static const char *inih_strerror(int err) {
    switch (err) {
        case INIH_EOK: return "No error";
        case INIH_ENOSECTION: return "No section found";
        case INIH_ENOKEY: return "No key found";
        case INIH_ENOVALUE: return "No value found";
        case INIH_EINVALIDVALUE: return "Invalid value";
        case INIH_EGENERIC: return "Generic error";
        case INIH_ESYNTAX: return "Syntax error";
        case INIH_EDUPSECTION: return "Duplicate section";
        case INIH_EDUPKEY: return "Duplicate key";
        case INIH_EUNKKEY: return "Unknown key";
        case INIH_EUNKSECTION: return "Unknown section";
        default: return "Unknown inih error code";
    }
}

static const char *lmdb_strerror(int err) {
    switch (err) {
        case LMDB_EOK: return "No error";
        case LMDB_EKEYEXIST: return "Key already exists";
        case LMDB_ENOTFOUND: return "Key not found";
        case LMDB_EPAGENOTFOUND: return "Page not found";
        case LMDB_ECORRUPTED: return "Database corrupted";
        case LMDB_EPANIC: return "Environment panic";
        case LMDB_EVERSION: return "Version mismatch";
        case LMDB_EINVALID: return "Invalid handle";
        case LMDB_EMAPFULL: return "Map full";
        case LMDB_EDBSFULL: return "Database full";
        case LMDB_EREADERSFULL: return "Readers full";
        case LMDB_ETXNFULL: return "Transaction full";
        case LMDB_ECURSORFULL: return "Cursor full";
        case LMDB_EPAGEFULL: return "Page full";
        case LMDB_EMAPRESIZED: return "Map resized";
        case LMDB_EINCOMPATIBLE: return "Incompatible environment";
        case LMDB_EBADRSLOT: return "Bad reader slot";
        case LMDB_EBADTXN: return "Bad transaction";
        case LMDB_EBADVALSIZE: return "Bad value size";
        case LMDB_EBADDBI: return "Bad database identifier";
        case LMDB_ECURSORUNINIT: return "Cursor uninitialized";
        case LMDB_ECURSORTHREAD: return "Cursor thread mismatch";
        case LMDB_ETXNREADONLY: return "Transaction read-only";
        case LMDB_EILLACCESS: return "Illegal access";
        default: return "Unknown error code";
    }
}

const char *po_strerror(int err) {
    if (IN(err, LMDB_EBASE, LMDB_ETOP)) 
        return lmdb_strerror(err);

    if (IN(err, INIH_EBASE, INIH_ETOP))
        return inih_strerror(err);

    return strerror(err);
}
