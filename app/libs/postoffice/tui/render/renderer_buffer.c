/**
 * @file renderer_buffer.c
 * @ingroup tui_render
 * @brief HOW: Framebuffer allocation, reuse, and damage computation.
 */

#include "render/renderer_buffer.h"

#include <stdlib.h>
#include <string.h>

struct tui_framebuffer {
    tui_surface a;
    tui_surface b;
    tui_surface *compose; /* next frame target */
    tui_surface *current; /* last completed frame */
    int width;
    int height;
    int full_damage_pending; /* 1 => next iterator yields full screen */
};

static void surface_free(tui_surface *s) {
    if (!s)
        return;
    free(s->cells);
    s->cells = NULL;
    s->width = 0;
    s->height = 0;
}

static int surface_alloc(tui_surface *s, int w, int h) {
    size_t n = (size_t)w * (size_t)h;
    tui_cell *cells = (tui_cell *)calloc(n, sizeof(tui_cell));
    if (!cells)
        return -1;
    s->width = w;
    s->height = h;
    s->cells = cells;
    return 0;
}

tui_framebuffer *tui_framebuffer_create(int width, int height, unsigned flags) {
    (void)flags;
    if (width <= 0)
        width = 80;
    if (height <= 0)
        height = 24;
    struct tui_framebuffer *fb = (struct tui_framebuffer *)calloc(1, sizeof(*fb));
    if (!fb)
        return NULL;
    if (surface_alloc(&fb->a, width, height) != 0 || surface_alloc(&fb->b, width, height) != 0) {
        surface_free(&fb->a);
        surface_free(&fb->b);
        free(fb);
        return NULL;
    }
    fb->compose = &fb->a;
    fb->current = &fb->b;
    fb->width = width;
    fb->height = height;
    fb->full_damage_pending = 1; /* first present should flush all */
    return fb;
}

void tui_framebuffer_destroy(tui_framebuffer *fb) {
    if (!fb)
        return;
    surface_free(&fb->a);
    surface_free(&fb->b);
    free(fb);
}

tui_surface *tui_framebuffer_begin(tui_framebuffer *fb) {
    return fb ? fb->compose : NULL;
}

bool tui_framebuffer_end(tui_framebuffer *fb) {
    if (!fb)
        return false;
    /* For now, always full-damage; future: compute diff of compose vs current */
    tui_surface *tmp = fb->current;
    fb->current = fb->compose;
    fb->compose = tmp;
    fb->full_damage_pending = 1;
    return true;
}

const tui_surface *tui_framebuffer_current(const tui_framebuffer *fb) {
    return fb ? fb->current : NULL;
}

bool tui_framebuffer_resize(tui_framebuffer *fb, int new_w, int new_h) {
    if (!fb)
        return false;
    if (new_w == fb->width && new_h == fb->height)
        return true;
    surface_free(&fb->a);
    surface_free(&fb->b);
    if (surface_alloc(&fb->a, new_w, new_h) != 0 || surface_alloc(&fb->b, new_w, new_h) != 0)
        return false;
    fb->compose = &fb->a;
    fb->current = &fb->b;
    fb->width = new_w;
    fb->height = new_h;
    fb->full_damage_pending = 1;
    return true;
}

struct fb_damage_iter {
    const struct tui_framebuffer *fb;
    int yielded;
};

fb_damage_iter *tui_framebuffer_damage_iter(const tui_framebuffer *fb) {
    if (!fb)
        return NULL;
    struct fb_damage_iter *it = (struct fb_damage_iter *)calloc(1, sizeof(*it));
    if (!it)
        return NULL;
    it->fb = fb;
    it->yielded = 0;
    return it;
}

bool tui_framebuffer_damage_next(fb_damage_iter *it, int *x, int *y, int *w, int *h) {
    if (!it || !it->fb)
        return false;
    if (it->yielded || !it->fb->full_damage_pending)
        return false;
    it->yielded = 1;
    if (x)
        *x = 0;
    if (y)
        *y = 0;
    if (w)
        *w = it->fb->width;
    if (h)
        *h = it->fb->height;
    return true;
}

void tui_framebuffer_damage_end(fb_damage_iter *it) {
    free(it);
}

/**
 * @file renderer_buffer.c
 * @ingroup tui_render
 * @brief HOW: Framebuffer allocation, reuse, and damage computation.
 *
 * Double buffering:
 *  - Maintain prev/current surfaces to compute cell diffs.
 *  - Track dirty rectangles to minimize writes in the backend.
 *
 * Memory:
 *  - Optionally allocate cell arrays from `perf` zero-copy pools.
 */

// Implementation will be added in subsequent milestones.
