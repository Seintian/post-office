#include "ui/ncurses_dyn.h"

/**
 * Robust dynamic loader for ncursesw.
 * Features:
 *  - Thread-safe one-time initialization (idempotent)
 *  - Optional override path parameter and environment variable PO_NCURSES_PATH
 *  - Candidate search list (Linux / macOS style names)
 *  - Detailed error diagnostics retrievable via po_ncurses_last_error()
 *  - Distinguishes between required / optional symbols; marks partial loads
 *  - Safe accessor returning NULL when unavailable
 *  - Optional explicit unload (primarily for tests / diagnostics)
 */

#include <dlfcn.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <errno.h>
#include <stdarg.h>

static po_ncurses_api g_api;                  /* Global symbol table */
static void *g_handle = NULL;                 /* dlopen handle        */
static _Atomic int g_state = 0;               /* 0 = uninit, 1 = ok, -1 = fail */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER; /* protects load/unload */
static char g_errbuf[256];                    /* last error message */

static inline void set_errorf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_errbuf, sizeof(g_errbuf), fmt, ap);
    va_end(ap);
}

typedef struct {
    const char *name;
    void **slot;
    bool required;
} sym_desc_t;

#define ADD_DESC(arr, idx, reqflag, sym) \
    arr[idx].name = #sym; \
    arr[idx].slot = (void**)&g_api.sym; \
    arr[idx].required = reqflag;

static int resolve_all(void) {
    size_t i = 0;
    /* Upper bound: count required + optional */
    sym_desc_t list[
#define COUNT_ONE(name, r, a) 1 +
        PO_NCURSES_REQUIRED_SYMBOLS(COUNT_ONE)
        PO_NCURSES_OPTIONAL_SYMBOLS(COUNT_ONE)
        0
    ];
#undef COUNT_ONE

#define ADD_REQ(name, ret, args) ADD_DESC(list, i++, true, name)
    PO_NCURSES_REQUIRED_SYMBOLS(ADD_REQ)
#undef ADD_REQ
#define ADD_OPT(name, ret, args) ADD_DESC(list, i++, false, name)
    PO_NCURSES_OPTIONAL_SYMBOLS(ADD_OPT)
#undef ADD_OPT

    g_api.partial_optional = false;

    for (size_t k = 0; k < i; k++) {
        /* Reset dlerror() before call */
        (void)dlerror();

        *list[k].slot = dlsym(g_handle, list[k].name);
        if (!*list[k].slot) {
            const char *dlerr = dlerror();
            if (list[k].required) {
                set_errorf(
                    "missing required symbol '%s'%s%s",
                    list[k].name,
                    dlerr
                        ? ": "
                        : "",
                    dlerr
                        ? dlerr
                        : ""
                );
                return -1;
            }
            else
                g_api.partial_optional = true;
        }
    }

    g_api.loaded = true;
    return 0;
}

int po_ncurses_load(const char *override_path, char *errbuf, size_t errsz) {
    pthread_mutex_lock(&g_lock);

    if (g_state != 0) { /* already decided */
        if (errbuf && errsz && g_state == -1)
            snprintf(errbuf, errsz, "%s", g_errbuf);

        pthread_mutex_unlock(&g_lock);
        return g_state == 1 ? 0 : -1;
    }

    memset(&g_api, 0, sizeof(g_api));
    g_errbuf[0] = '\0';

    const char *env_path = getenv("PO_NCURSES_PATH");
    const char *candidates[] = {
        override_path,
        env_path,
        /* Common Linux sonames */
        "libncursesw.so.6",
        "libncursesw.so.5", /* legacy */
        "libncursesw.so",
        /* macOS homebrew / opt paths might need absolute path by caller */
        "ncursesw",
        NULL
    };

    char attempts[256] = {0};
    size_t attempts_len = 0;

    for (int idx = 0; candidates[idx]; idx++) {
        const char *cand = candidates[idx];
        if (!cand || !*cand)
            continue;

        if (attempts_len + strlen(cand) + 2 < sizeof(attempts))
            attempts_len += (size_t)snprintf(
                attempts + attempts_len, sizeof(attempts) - attempts_len, "%s ", cand
            );

        g_handle = dlopen(cand, RTLD_NOW | RTLD_LOCAL);
        if (!g_handle)
            continue; /* try next */

        if (resolve_all() == 0) {
            g_state = 1;
            if (errbuf && errsz)
                errbuf[0] = '\0';

            pthread_mutex_unlock(&g_lock);
            return 0;
        }

        /* failed symbol resolution */
        dlclose(g_handle);
        g_handle = NULL;
    }

    if (g_errbuf[0] == '\0')
        set_errorf(
            "ncursesw load failed (candidates tried: %s)",
            attempts[0] ? attempts : "<none>"
        );

    g_state = -1;
    if (errbuf && errsz)
        snprintf(errbuf, errsz, "%s", g_errbuf);

    pthread_mutex_unlock(&g_lock);
    return -1;
}

const po_ncurses_api *po_ncurses(void) {
    if (atomic_load(&g_state) == 0) {
        /* Lazy attempt (ignore errors) */
        po_ncurses_load(NULL, NULL, 0);
    }

    return (g_state == 1) ? &g_api : NULL;
}

const char *po_ncurses_last_error(void) {
    if (atomic_load(&g_state) == -1)
        return g_errbuf[0] ? g_errbuf : "ncursesw not available";

    return NULL;
}

int po_ncurses_unload(void) {
    pthread_mutex_lock(&g_lock);

    if (g_state != 1) { /* not loaded */
        pthread_mutex_unlock(&g_lock);
        return -1;
    }

    if (g_handle) {
        dlclose(g_handle);
        g_handle = NULL;
    }

    memset(&g_api, 0, sizeof(g_api));
    g_state = 0; /* allow future reload */
    pthread_mutex_unlock(&g_lock);
    return 0;
}
