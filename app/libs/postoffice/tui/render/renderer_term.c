/**
 * @file renderer_term.c
 * @ingroup tui_render
 * @brief HOW: Ncurses backend implementation details and flush algorithm.
 *
 * Initialization:
 *  - initscr(), cbreak(), noecho(), keypad(stdscr, TRUE), curs_set(0)
 *  - Optionally nodelay() based on loop strategy.
 *
 * Present:
 *  - For P1, brute-force write all cells via mvaddch(); P2 adds diffing.
 *  - Respect clipping when frame size differs from terminal size.
 *
 * Cleanup:
 *  - endwin() on shutdown; ensure idempotency.
 */

#include "render/renderer_term.h"

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

struct tui_renderer_term {
    int width;
    int height;
    int nonblocking;
    int initialized;
};

static void detect_size(struct tui_renderer_term *t) {
    if (!t)
        return;
    int h = 0, w = 0;
    getmaxyx(stdscr, h, w);
    t->width = w;
    t->height = h;
}

tui_renderer_term *tui_renderer_term_create(int width, int height, unsigned flags) {
    (void)width;
    (void)height;
    struct tui_renderer_term *t = (struct tui_renderer_term *)calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    if (flags & TUI_TERM_NONBLOCKING)
        nodelay(stdscr, TRUE);
    t->nonblocking = (flags & TUI_TERM_NONBLOCKING) ? 1 : 0;
    t->initialized = 1;
    start_color();
    use_default_colors();
    detect_size(t);
    return t;
}

void tui_renderer_term_destroy(tui_renderer_term *t) {
    if (!t)
        return;
    if (t->initialized) {
        endwin();
        t->initialized = 0;
    }
    free(t);
}

int tui_renderer_term_query_size(tui_renderer_term *t, int *out_w, int *out_h) {
    if (!t)
        return -1;
    detect_size(t);
    if (out_w)
        *out_w = t->width;
    if (out_h)
        *out_h = t->height;
    return 0;
}

int tui_renderer_term_present(tui_renderer_term *t, const tui_surface *frame) {
    if (!t || !frame)
        return -1;
    int h = t->height < frame->height ? t->height : frame->height;
    int w = t->width < frame->width ? t->width : frame->width;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const tui_cell *c = &frame->cells[y * frame->width + x];
            mvaddch(y, x, (chtype)(c->ch & 0xFF));
        }
    }
    refresh();
    return 0;
}
