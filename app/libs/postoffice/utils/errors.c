#include "utils/errors.h"

#include <string.h>

#define BETWEEN(x, base, top) ((x) >= (base) && (x) <= (top))

static const char *inih_strerror(int err) {
    switch (err) {
    case INIH_EOK:
        return "No error";
    case INIH_ENOSECTION:
        return "No section found";
    case INIH_ENOKEY:
        return "No key found";
    case INIH_ENOVALUE:
        return "No value found";
    case INIH_EINVAL:
        return "Invalid value";
    case INIH_EGENERIC:
        return "Generic error";
    case INIH_ESYNTAX:
        return "Syntax error";
    case INIH_EDUPSECTION:
        return "Duplicate section";
    case INIH_EDUPKEY:
        return "Duplicate key";
    case INIH_EUNKKEY:
        return "Unknown key";
    case INIH_EUNKSECTION:
        return "Unknown section";
    case INIH_NOTFOUND:
        return "Key or section not found";
    case INIH_ENOUSER:
        return "No user provided for handler";
    case INIH_ERANGE:
        return "Value out of range";
    default:
        return "Unknown inih error code";
    }
}

static const char *lmdb_strerror(int err) {
    switch (err) {
    case DB_EOK:
        return "No error";
    case DB_EKEYEXIST:
        return "Key already exists";
    case DB_ENOTFOUND:
        return "Key not found";
    case DB_EPAGENOTFOUND:
        return "Page not found";
    case DB_ECORRUPTED:
        return "Database corrupted";
    case DB_EPANIC:
        return "Environment panic";
    case DB_EVERSION:
        return "Version mismatch";
    case DB_EINVALID:
        return "Invalid handle";
    case DB_EMAPFULL:
        return "Map full";
    case DB_EDBSFULL:
        return "Database full";
    case DB_EREADERSFULL:
        return "Readers full";
    case DB_ETXNFULL:
        return "Transaction full";
    case DB_ECURSORFULL:
        return "Cursor full";
    case DB_EPAGEFULL:
        return "Page full";
    case DB_EMAPRESIZED:
        return "Map resized";
    case DB_EINCOMP:
        return "Incompatible environment";
    case DB_EBADRSLOT:
        return "Bad reader slot";
    case DB_EBADTXN:
        return "Bad transaction";
    case DB_EBADVALSIZE:
        return "Bad value size";
    case DB_EBADDBI:
        return "Bad database identifier";
    case DB_ECURSORUNINIT:
        return "Cursor uninitialized";
    case DB_ECURSORTHREAD:
        return "Cursor thread mismatch";
    case DB_ETXNREADONLY:
        return "Transaction read-only";
    case DB_EILLACC:
        return "Illegal access";
    default:
        return "Unknown error code";
    }
}

static const char *perf_strerror(int err) {
    switch (err) {
    case PERF_EOK:
        return "No error";
    case PERF_EINVAL:
        return "Invalid argument";
    case PERF_ENOCOUNTER:
        return "Counter not found";
    case PERF_ENOTIMER:
        return "Timer not found";
    case PERF_ENOHISTOGRAM:
        return "Histogram not found";
    case PERF_EALREADY:
        return "Already initialized";
    case PERF_ENOTINIT:
        return "Performance subsystem not initialized";
    default:
        return "Unknown performance error code";
    }
}

static const char *net_strerror(int err) {
    switch (err) {
    case NET_EOK:
        return "No error";
    case NET_EINVAL:
        return "Invalid argument";
    case NET_ENOSPC:
        return "No space left on device";
    case NET_EMSGSIZE:
        return "Message too long";
    case NET_ESOCK:
        return "Socket error";
    case NET_ETIMEOUT:
        return "Operation timed out";
    case NET_EVERSION:
        return "Protocol version mismatch";
    case NET_EAGAIN:
        return "Resource temporarily unavailable";
    case NET_ENOTCONN:
        return "Socket is not connected";
    case NET_ECONNREFUSED:
        return "Connection refused";
    case NET_EPROTO:
        return "Protocol error";
    case NET_EPROTONOSUPPORT:
        return "Protocol not supported";
    case NET_EALREADY:
        return "Network already initialized or in use";
    case NET_ENOENT:
        return "No such file or directory";
    case NET_EIO:
        return "I/O error";
    default:
        return "Unknown network error code";
    }
}

static const char *zcp_strerror(int err) {
    switch (err) {
    case ZCP_EOK:
        return "No error";
    case ZCP_EINVAL:
        return "Invalid argument";
    case ZCP_ENOMEM:
        return "Out of memory";
    case ZCP_EAGAIN:
        return "Resource temporarily unavailable";
    case ZCP_EMMAP:
        return "Memory mapping failed";
    default:
        return "Unknown zero-copy pool error code";
    }
}

const char *po_strerror(int err) {
    if (BETWEEN(err, DB_EBASE, DB_ETOP))
        return lmdb_strerror(err);

    if (BETWEEN(err, INIH_EBASE, INIH_ETOP))
        return inih_strerror(err);

    if (BETWEEN(err, PERF_EBASE, PERF_ETOP))
        return perf_strerror(err);

    if (BETWEEN(err, NET_EBASE, NET_ETOP))
        return net_strerror(err);

    if (BETWEEN(err, ZCP_EBASE, ZCP_ETOP))
        return zcp_strerror(err);

    return strerror(err);
}
