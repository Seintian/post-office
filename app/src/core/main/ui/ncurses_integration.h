/* ncurses_integration.h - High-level helpers wrapping dynamic ncurses loader
 *
 * Provides a thin convenience layer to initialize and shutdown a ncurses
 * user interface using the dynamically loaded symbol table. These helpers
 * are optional and degrade gracefully when ncurses is not present at runtime.
 */
#ifndef PO_NCURSES_INTEGRATION_H
#define PO_NCURSES_INTEGRATION_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bit flags controlling boot behaviour */
enum {
    PO_NCURSES_UI_FLAG_NONBLOCK     = 1u << 0, /* make getch non-blocking */
    PO_NCURSES_UI_FLAG_HIDE_CURSOR  = 1u << 1, /* hide cursor */
    PO_NCURSES_UI_FLAG_ENABLE_COLOR = 1u << 2  /* call start_color / use_default_colors */
};

/* Returns 0 on success, -1 if ncurses not available or initialization failed.
 * On failure, errbuf (if provided) receives message. Idempotent: additional
 * calls after success return 0. */
int po_ncurses_ui_boot(unsigned flags, char *errbuf, size_t errsz);

/* Gracefully shutdown UI (if booted). Safe to call multiple times. */
void po_ncurses_ui_shutdown(void);

/* True if UI booted successfully. */
bool po_ncurses_ui_active(void);

#ifdef __cplusplus
}
#endif

#endif /* PO_NCURSES_INTEGRATION_H */
