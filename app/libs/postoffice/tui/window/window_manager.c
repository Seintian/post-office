/**
 * @file window_manager.c
 * @ingroup tui_window
 * @brief HOW: Manage viewport and resize hooks.
 */

#include "window/window_manager.h"

#include <stdlib.h>

struct window_manager {
    int x, y, w, h;
};

struct window_manager *window_manager_create(struct ui_context *ctx) {
    (void)ctx;
    return (struct window_manager *)calloc(1, sizeof(struct window_manager));
}
void window_manager_destroy(struct window_manager *wm) {
    free(wm);
}

void window_manager_get_viewport(const struct window_manager *wm, int *out_x, int *out_y,
                                 int *out_w, int *out_h) {
    if (!wm)
        return;
    if (out_x)
        *out_x = wm->x;
    if (out_y)
        *out_y = wm->y;
    if (out_w)
        *out_w = wm->w;
    if (out_h)
        *out_h = wm->h;
}
void window_manager_set_viewport(struct window_manager *wm, int x, int y, int w, int h) {
    if (!wm)
        return;
    wm->x = x;
    wm->y = y;
    wm->w = w;
    wm->h = h;
}
void window_manager_on_resize(struct window_manager *wm, int new_w, int new_h) {
    if (!wm)
        return;
    wm->w = new_w;
    wm->h = new_h;
}
