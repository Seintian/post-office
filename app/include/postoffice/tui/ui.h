/**
 * @file ui.h
 * @brief Minimal public API for the postoffice TUI subsystem (early scaffold).
 *
 * This initial version intentionally exposes a very small surface so that
 * higher level modules can start integrating while the richer architecture
 * (layout, styled text, event plumbing, real terminal backend) is developed.
 *
 * Design goals (phase 1):
 *  - Deterministic off-screen buffer renderer for tests
 *  - Simple fixed-size grid (monospaced cells)
 *  - Basic label primitive
 *  - Explicit snapshot API to allow assertions in unit tests
 *
 * Future expansions (not yet implemented):
 *  - Diffed terminal renderer with raw mode handling
 *  - Event loop & input decoding
 *  - Layout / style / theming / accessibility layers
 */

#ifndef POSTOFFICE_TUI_UI_H
#define POSTOFFICE_TUI_UI_H

#include <stddef.h>
#include <stdint.h>

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque types are declared in types.h */

typedef struct po_tui_config {
    int width_override;  /* <=0 => detect / default */
    int height_override; /* <=0 => detect / default */
    unsigned flags;      /* reserved for future (e.g., disable_term) */
} po_tui_config;

enum {
    PO_TUI_FLAG_DISABLE_TERM = 0x1u /* force off-screen buffer only */
};

/**
 * @brief Create and initialize a TUI application context.
 * @param out_app (output) created handle
 * @param cfg optional configuration (NULL => defaults)
 * @return 0 on success, -1 on failure
 */
int po_tui_init(po_tui_app **out_app, const po_tui_config *cfg);

/**
 * @brief Destroy and free a TUI application context.
 */
void po_tui_shutdown(po_tui_app *app);

/**
 * @brief Add a simple immutable label to the scene.
 * The text is copied internally.
 * @param app context
 * @param x column (0-based)
 * @param y row (0-based)
 * @param text UTF-8 (currently treated as bytes, no wide char support yet)
 * @return id (>=0) on success, -1 on failure
 */
int po_tui_add_label(po_tui_app *app, int x, int y, const char *text);

/**
 * @brief Render current scene into active backend (buffer or terminal).
 * For the initial scaffold this always targets the off-screen buffer.
 * @return 0 on success, -1 on failure
 */
int po_tui_render(po_tui_app *app);

/**
 * @brief Obtain a textual snapshot (lines separated by '\n').
 * Buffer is truncated to (out_size-1) and always NUL-terminated.
 * @param app context
 * @param out buffer
 * @param out_size size of out
 * @param written optional number of bytes (excluding NUL) actually written
 * @return 0 on success, -1 on failure
 */
int po_tui_snapshot(const po_tui_app *app, char *out, size_t out_size, size_t *written);

/**
 * Root composition: set/get the root widget tree.
 */
int po_tui_set_root(po_tui_app *app, po_tui_widget *root);
po_tui_widget *po_tui_get_root(po_tui_app *app);

/**
 * Event loop helpers (optional): single-step tick and render, if app wants.
 */
int po_tui_tick(po_tui_app *app, double frame_budget_ms);
int po_tui_present(po_tui_app *app);

#ifdef __cplusplus
}
#endif

#endif /* POSTOFFICE_TUI_UI_H */
