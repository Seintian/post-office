/**
 * @file symbols.h
 * @ingroup tui_theme
 * @brief Symbol characters for borders, bullets, progress fills, etc.
 */

#ifndef POSTOFFICE_TUI_THEME_SYMBOLS_H
#define POSTOFFICE_TUI_THEME_SYMBOLS_H

#include <stdint.h>

struct tui_symbols; /* opaque */

typedef struct tui_borders {
    uint32_t horiz, vert;
    uint32_t tl, tr, bl, br; /* corners */
} tui_borders;

typedef struct tui_symbols_view {
    tui_borders box;
    uint32_t bullet;
    uint32_t progress_empty;
    uint32_t progress_full;
} tui_symbols_view;

/* Get a read-only view of current symbols set */
const tui_symbols_view *tui_symbols_view_current(const struct tui_symbols *s);

#endif /* POSTOFFICE_TUI_THEME_SYMBOLS_H */
