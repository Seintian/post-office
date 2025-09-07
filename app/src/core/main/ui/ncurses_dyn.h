#ifndef PO_NCURSES_DYN_H
#define PO_NCURSES_DYN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declare WINDOW to avoid forcing inclusion of curses.h everywhere */
typedef struct _win WINDOW;

/* List macro for maintainability */
#define PO_NCURSES_REQUIRED_SYMBOLS(X) \
    X(initscr,            WINDOW*, (void)) \
    X(endwin,             int,     (void)) \
    X(newwin,             WINDOW*, (int,int,int,int)) \
    X(delwin,             int,     (WINDOW*)) \
    X(wrefresh,           int,     (WINDOW*)) \
    X(wclear,             int,     (WINDOW*)) \
    X(werase,             int,     (WINDOW*)) \
    X(waddnstr,           int,     (WINDOW*, const char*, int)) \
    X(wgetch,             int,     (WINDOW*)) \
    X(keypad,             int,     (WINDOW*, bool)) \
    X(curs_set,           int,     (int)) \
    X(nodelay,            int,     (WINDOW*, bool)) \
    X(noecho,             int,     (void)) \
    X(cbreak,             int,     (void)) \
    X(start_color,        int,     (void)) \
    X(init_pair,          int,     (short, short, short)) \
    X(has_colors,         bool,    (void)) \
    X(wattron,            int,     (WINDOW*, int)) \
    X(wattroff,           int,     (WINDOW*, int)) \
    X(getch,              int,     (void))

#define PO_NCURSES_OPTIONAL_SYMBOLS(X) \
    X(resize_term,        int,     (int,int)) \
    X(wresize,            int,     (WINDOW*, int, int)) \
    X(use_default_colors, int, (void))

typedef struct po_ncurses_api {
#define MAKE_FIELD(name, rettype, args) rettype (*name) args;
    PO_NCURSES_REQUIRED_SYMBOLS(MAKE_FIELD)
    PO_NCURSES_OPTIONAL_SYMBOLS(MAKE_FIELD)
#undef MAKE_FIELD

    /* Status flags */
    bool loaded;          /* Entire table ready */
    bool partial_optional;/* Some optional missing */
} po_ncurses_api;

/* Initializes (idempotent). Returns 0 on success, <0 on failure */
int po_ncurses_load(const char *override_path, char *errbuf, size_t errbuf_sz);

/* Accessor: returns NULL if not loaded */
const po_ncurses_api *po_ncurses(void);

/* Returns last loader error (thread-safe string) or NULL if none */
const char *po_ncurses_last_error(void);

/* Explicitly unload the ncurses library. Only call when no TUI code is in use.
 * Returns 0 on success, -1 if library was not loaded or still in use. */
int po_ncurses_unload(void);

/* Shortcut predicate */
static inline bool po_ncurses_enabled(void) {
    const po_ncurses_api *api = po_ncurses();
    return api && api->loaded;
}

/* Example wrapper (optional): safer than sprinkling direct pointer derefs */
static inline WINDOW *po_ncurses_initscr(void) {
    const po_ncurses_api *api = po_ncurses();
    return (api && api->initscr) ? api->initscr() : NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* PO_NCURSES_DYN_H */
