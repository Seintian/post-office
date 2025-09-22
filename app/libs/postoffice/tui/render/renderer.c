/**
 * @file renderer.c
 * @ingroup tui_render
 * @brief HOW: Renderer facade selecting backend and mediating presentation.
 *
 * Chooses between terminal backend and no-op/test buffer based on flags.
 * Owns lifetime of underlying backend and shares a framebuffer instance.
 */

#include "render/renderer.h"

#include <stdlib.h>
#include <string.h>

#include "internal/config_internal.h"
#include "render/renderer_buffer.h"
#include "render/renderer_term.h"

struct tui_renderer {
    tui_renderer_term *term;
    tui_framebuffer *fb;
    unsigned caps;
    tui_log_fn log_cb;
    void *log_ud;
};

/* static void log_msg(struct tui_renderer *r, const char *msg) { if (r && r->log_cb)
 * r->log_cb(r->log_ud, msg); } */

tui_renderer *tui_renderer_create(const tui_renderer_config *cfg, tui_log_fn log_cb, void *log_ud) {
    (void)cfg;
    struct tui_renderer *r = (struct tui_renderer *)calloc(1, sizeof(*r));
    if (!r)
        return NULL;
    r->log_cb = log_cb;
    r->log_ud = log_ud;
#if PO_TUI_WITH_NCURSES
    int iw = cfg && cfg->width > 0 ? cfg->width : 0;
    int ih = cfg && cfg->height > 0 ? cfg->height : 0;
    r->term = tui_renderer_term_create(iw, ih, 0);
    if (!r->term) {
        free(r);
        return NULL;
    }
    int tw = iw, th = ih;
    (void)tui_renderer_term_query_size(r->term, &tw, &th);
    r->fb = tui_framebuffer_create(tw, th, 0);
    if (!r->fb) {
        tui_renderer_term_destroy(r->term);
        free(r);
        return NULL;
    }
    r->caps = TUI_REN_CAP_COLOR256 | TUI_REN_CAP_BOLD | TUI_REN_CAP_UNDERLINE;
#else
    (void)log_msg;
    free(r);
    return NULL;
#endif
    return r;
}

void tui_renderer_destroy(tui_renderer *r) {
    if (!r)
        return;
    tui_framebuffer_destroy(r->fb);
    tui_renderer_term_destroy(r->term);
    free(r);
}

int tui_renderer_query_size(tui_renderer *r, int *out_w, int *out_h) {
    if (!r)
        return TUI_REN_EINVAL;
    int w = 0, h = 0;
    (void)tui_renderer_term_query_size(r->term, &w, &h);
    if (out_w)
        *out_w = w;
    if (out_h)
        *out_h = h;
    return TUI_REN_OK;
}

unsigned tui_renderer_capabilities(const tui_renderer *r) {
    return r ? r->caps : 0;
}

int tui_renderer_present(tui_renderer *r, const tui_surface *frame, unsigned flags) {
    if (!r || !frame)
        return TUI_REN_EINVAL;
    int w = 0, h = 0;
    tui_renderer_term_query_size(r->term, &w, &h);
    if (w != frame->width || h != frame->height) {
        /* try to resize framebuffer and use terminal size */
        (void)tui_framebuffer_resize(r->fb, w, h);
    }
    /* Copy provided frame into compose buffer to own it for diffing */
    tui_surface *compose = tui_framebuffer_begin(r->fb);
    if (!compose)
        return TUI_REN_ENOMEM;
    int cw = compose->width < frame->width ? compose->width : frame->width;
    int ch = compose->height < frame->height ? compose->height : frame->height;
    for (int y = 0; y < ch; ++y) {
        memcpy(&compose->cells[y * compose->width], &frame->cells[y * frame->width],
               (size_t)cw * sizeof(tui_cell));
    }
    tui_framebuffer_end(r->fb);
    const tui_surface *cur = tui_framebuffer_current(r->fb);
    (void)flags; /* future: respect TUI_PRESENT_FORCE_FULL/SYNC */
    int rc = tui_renderer_term_present(r->term, cur);
    return rc == 0 ? TUI_REN_OK : TUI_REN_EBACKEND;
}
