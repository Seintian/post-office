/**
 * @file screen_int.h
 * @ingroup tui_core
 * @brief INTERNAL: Screen interface between app screens and engine.
 */

#ifndef POSTOFFICE_TUI_INTERNAL_SCREEN_INT_H
#define POSTOFFICE_TUI_INTERNAL_SCREEN_INT_H

/* Internal only; screen vtable contract */

struct ui_context;
struct ui_event;
struct po_tui_widget;

typedef struct ui_screen_vtable {
    /* Called once when screen becomes active */
    void (*on_enter)(void *self, struct ui_context *ctx);
    /* Called once when screen is deactivated */
    void (*on_exit)(void *self, struct ui_context *ctx);
    /* Handle events (keys, mouse, timers) */
    void (*on_event)(void *self, struct ui_context *ctx, const struct ui_event *ev);
    /* Provide root widget for layout/render */
    struct po_tui_widget *(*root)(void *self);
} ui_screen_vtable;

typedef struct ui_screen {
    const ui_screen_vtable *vt;
    void *impl;
} ui_screen;

#endif /* POSTOFFICE_TUI_INTERNAL_SCREEN_INT_H */
