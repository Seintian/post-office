/**
 * @file layout_engine.h
 * @ingroup tui_layout
 * @brief Layout engine dispatch interface.
 *
 * WHAT:
 *  - Computes positions/sizes for a widget tree given constraints.
 *  - Supports multiple strategies: flex, grid, stack; pluggable per node.
 *
 * HOW (see `layout_engine.c`):
 *  - Walks tree, calling measure/layout callbacks on widgets.
 *  - Uses dirty flags to avoid re-measure when unchanged.
 */

#ifndef POSTOFFICE_TUI_LAYOUT_ENGINE_H
#define POSTOFFICE_TUI_LAYOUT_ENGINE_H

struct ui_context;    /* fwd */
struct po_tui_widget; /* fwd */

typedef enum layout_dirty_flags {
    LAYOUT_DIRTY_NONE = 0,
    LAYOUT_DIRTY_MEASURE = 1u << 0,
    LAYOUT_DIRTY_CHILDREN = 1u << 1,
    LAYOUT_DIRTY_STYLE = 1u << 2,
} layout_dirty_flags;

typedef struct layout_constraints {
    int min_w, min_h;
    int max_w, max_h; /* <=0 means unbounded */
} layout_constraints;

typedef struct layout_engine_config {
    unsigned flags; /* reserved */
} layout_engine_config;

/** Compute layout for the widget tree. Returns 0 on success. */
int layout_compute(struct ui_context *ctx, struct po_tui_widget *root);

/** Mark subtree dirty for re-layout. */
void layout_mark_dirty(struct po_tui_widget *node, unsigned dirty_flags);

#endif /* POSTOFFICE_TUI_LAYOUT_ENGINE_H */
