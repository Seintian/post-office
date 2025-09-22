/**
 * @file renderer_buffer.h
 * @ingroup tui_render
 * @brief Off-screen frame buffers and optional diff/damage tracking.
 *
 * WHAT: Manages current/previous `tui_surface` buffers.
 * HOW (see `renderer_buffer.c`):
 *  - Allocates cell arrays from zero-copy pools (perf) when configured.
 *  - Computes changed regions to minimize terminal writes.
 */

#ifndef POSTOFFICE_TUI_RENDER_RENDERER_BUFFER_H
#define POSTOFFICE_TUI_RENDER_RENDERER_BUFFER_H

#include <stdbool.h>

#include "renderer.h"

typedef struct tui_framebuffer tui_framebuffer;

tui_framebuffer *tui_framebuffer_create(int width, int height, unsigned flags);
void tui_framebuffer_destroy(tui_framebuffer *fb);

/** Get mutable surface for composing the next frame. */
tui_surface *tui_framebuffer_begin(tui_framebuffer *fb);

/** Finish frame; compute damage and retain prev for next diff. */
bool tui_framebuffer_end(tui_framebuffer *fb);

/** Access last completed frame (read-only). */
const tui_surface *tui_framebuffer_current(const tui_framebuffer *fb);

/** Resize framebuffer surfaces, dropping contents; returns false on OOM. */
bool tui_framebuffer_resize(tui_framebuffer *fb, int new_w, int new_h);

/** Iterate over damaged regions since previous frame */
typedef struct fb_damage_iter fb_damage_iter;
fb_damage_iter *tui_framebuffer_damage_iter(const tui_framebuffer *fb);
bool tui_framebuffer_damage_next(fb_damage_iter *it, int *x, int *y, int *w, int *h);
void tui_framebuffer_damage_end(fb_damage_iter *it);

#endif /* POSTOFFICE_TUI_RENDER_RENDERER_BUFFER_H */
