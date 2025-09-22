/**
 * @file palette.h
 * @ingroup tui_theme
 * @brief Named color roles used by widgets and renderer.
 */

#ifndef POSTOFFICE_TUI_THEME_PALETTE_H
#define POSTOFFICE_TUI_THEME_PALETTE_H

struct tui_palette; /* opaque */

typedef enum tui_color_role {
    TUI_COL_FG = 0,
    TUI_COL_BG,
    TUI_COL_ACCENT,
    TUI_COL_MUTED,
    TUI_COL_WARN,
    TUI_COL_ERROR,
    TUI_COL_OK,
    TUI_COL_COUNT
} tui_color_role;

/* Get 16-bit RGB components (0..65535) for a role; returns false if unset */
bool tui_palette_get_rgb(const struct tui_palette *p, tui_color_role role, unsigned *r, unsigned *g,
                         unsigned *b);
bool tui_palette_set_rgb(struct tui_palette *p, tui_color_role role, unsigned r, unsigned g,
                         unsigned b);

#endif /* POSTOFFICE_TUI_THEME_PALETTE_H */
