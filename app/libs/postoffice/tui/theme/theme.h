/**
 * @file theme.h
 * @ingroup tui_theme
 * @brief Theme API (palette and symbols mapping).
 */

#ifndef POSTOFFICE_TUI_THEME_THEME_H
#define POSTOFFICE_TUI_THEME_THEME_H

struct tui_palette; /* opaque */
struct tui_symbols; /* opaque */

/* Create default palette/symbols instances (owned by caller) */
struct tui_palette *tui_palette_default(void);
struct tui_symbols *tui_symbols_default(void);

/* Global theme instances (shared) */
void tui_theme_set_palette(struct tui_palette *p);
void tui_theme_set_symbols(struct tui_symbols *s);
const struct tui_palette *tui_theme_palette(void);
const struct tui_symbols *tui_theme_symbols(void);

#endif /* POSTOFFICE_TUI_THEME_THEME_H */
