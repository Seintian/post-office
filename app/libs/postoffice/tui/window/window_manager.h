/**
 * @file window_manager.h
 * @ingroup tui_window
 * @brief Window/viewport management API (planned minimal stubs).
 */

#ifndef POSTOFFICE_TUI_WINDOW_MANAGER_H
#define POSTOFFICE_TUI_WINDOW_MANAGER_H

struct window_manager; /* opaque */
struct ui_context;     /* fwd */

struct window_manager *window_manager_create(struct ui_context *ctx);
void window_manager_destroy(struct window_manager *wm);

void window_manager_get_viewport(const struct window_manager *wm, int *out_x, int *out_y,
                                 int *out_w, int *out_h);
void window_manager_set_viewport(struct window_manager *wm, int x, int y, int w, int h);
void window_manager_on_resize(struct window_manager *wm, int new_w, int new_h);

#endif /* POSTOFFICE_TUI_WINDOW_MANAGER_H */
