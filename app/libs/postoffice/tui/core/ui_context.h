/**
 * @file ui_context.h
 * @ingroup tui_core
 * @brief Central engine context handle and creation/destruction API.
 *
 * WHAT: Declares the opaque `ui_context` and lifecycle functions.
 * HOW (see `ui_context.c`):
 *  - Owns registries, active screen, renderer backend, draw batch, and queues.
 *  - Integrates `perf` ring buffers (events) and batchers (draw ops).
 */

#ifndef POSTOFFICE_TUI_CORE_UI_CONTEXT_H
#define POSTOFFICE_TUI_CORE_UI_CONTEXT_H

#include <stdbool.h>

struct ui_context;

/** Engine construction parameters */
typedef struct ui_context_config {
    int width_override;  /**< <=0: auto detect */
    int height_override; /**< <=0: auto detect */
    unsigned flags;      /**< e.g., disable_term */
} ui_context_config;

/** Create a new TUI context. */
bool ui_context_create(struct ui_context **out, const ui_context_config *cfg);

/** Destroy and free a TUI context. */
void ui_context_destroy(struct ui_context *ctx);

/** Query current context size (cells). Returns 0 on success. */
int ui_context_size(const struct ui_context *ctx, int *out_w, int *out_h);

/** Request a present at next loop iteration (wake-up hint). */
void ui_context_request_present(struct ui_context *ctx);

#endif /* POSTOFFICE_TUI_CORE_UI_CONTEXT_H */
