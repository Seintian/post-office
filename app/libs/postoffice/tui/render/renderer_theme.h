/**
 * @file renderer_theme.h
 * @ingroup tui_render
 * @brief WHAT: Mapping from logical styles to terminal attributes.
 */

#ifndef POSTOFFICE_TUI_RENDER_RENDERER_THEME_H
#define POSTOFFICE_TUI_RENDER_RENDERER_THEME_H

struct renderer_theme; /* opaque */

struct renderer_theme *renderer_theme_create(void);
void renderer_theme_destroy(struct renderer_theme *th);

/** Map logical color/style to backend attributes (implementation-defined). */
unsigned renderer_theme_attr_color(struct renderer_theme *th, unsigned fg, unsigned bg);
unsigned renderer_theme_attr_style(struct renderer_theme *th, unsigned attrs);

#endif /* POSTOFFICE_TUI_RENDER_RENDERER_THEME_H */
