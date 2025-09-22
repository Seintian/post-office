/**
 * @file renderer_term.h
 * @ingroup tui_render
 * @brief Ncurses-based terminal backend for flushing cell surfaces.
 *
 * HOW: Implements initialization (raw mode), size queries, and frame flush
 * using `mvaddch()`/`addch()` with a future diff path. Integrates with
 * `draw_batch` coalescing and can accept damage regions for efficiency.
 */

#ifndef POSTOFFICE_TUI_RENDER_RENDERER_TERM_H
#define POSTOFFICE_TUI_RENDER_RENDERER_TERM_H

#include "renderer.h"

typedef struct tui_renderer_term tui_renderer_term;

enum tui_renderer_term_flags { TUI_TERM_NONBLOCKING = 1u << 0 };

tui_renderer_term *tui_renderer_term_create(int width, int height, unsigned flags);
void tui_renderer_term_destroy(tui_renderer_term *t);
int tui_renderer_term_query_size(tui_renderer_term *t, int *out_w, int *out_h);
int tui_renderer_term_present(tui_renderer_term *t, const tui_surface *frame);

#endif /* POSTOFFICE_TUI_RENDER_RENDERER_TERM_H */
