/**
 * @file ui_loop.h
 * @ingroup tui_core
 * @brief Frame loop interface coordinating input, events, layout, and rendering.
 *
 * WHAT: Declares the public functions to tick the TUI engine.
 *  - `tui_tick` processes timers, polls input, dispatches events/commands.
 *  - `tui_render` performs layout and flushes the renderer backend.
 *
 * HOW (see `ui_loop.c`):
 *  - Integrates with `perf` ring buffers for the event queue.
 *  - Uses `perf` batchers to coalesce draw operations prior to flush.
 *  - Zero-copy pools can be injected for large buffers (text spans, frames).
 *
 * Error handling: boolean return codes, with future `tui_last_error()` for details.
 */

#ifndef POSTOFFICE_TUI_CORE_UI_LOOP_H
#define POSTOFFICE_TUI_CORE_UI_LOOP_H

#include <stdbool.h>

struct ui_context;

/**
 * @brief Process one engine tick under a frame budget.
 * @param ctx Engine context
 * @param frame_budget_ms Soft budget; use for timers, not strict sleeping
 * @return true on success, false on fatal error
 */
bool tui_tick(struct ui_context *ctx, double frame_budget_ms);

/**
 * @brief Compute layout if dirty and render current scene.
 * @param ctx Engine context
 * @return true on success, false on fatal error
 */
bool tui_render_frame(struct ui_context *ctx);

#endif /* POSTOFFICE_TUI_CORE_UI_LOOP_H */
