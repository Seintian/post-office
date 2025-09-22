/**
 * @file ui_state.h
 * @ingroup tui_core
 * @brief WHAT: Engine UI state model and dirty flags.
 */

#ifndef POSTOFFICE_TUI_CORE_UI_STATE_H
#define POSTOFFICE_TUI_CORE_UI_STATE_H

struct ui_state;   /* opaque */
struct ui_context; /* fwd */

struct ui_state *ui_state_create(struct ui_context *ctx);
void ui_state_destroy(struct ui_state *st);

typedef enum ui_state_dirty {
    UI_DIRTY_NONE = 0,
    UI_DIRTY_LAYOUT = 1u << 0,
    UI_DIRTY_STYLE = 1u << 1,
    UI_DIRTY_RENDER = 1u << 2,
} ui_state_dirty;

void ui_state_mark_dirty(struct ui_state *st, unsigned flags);
unsigned ui_state_dirty_flags(const struct ui_state *st);

#endif /* POSTOFFICE_TUI_CORE_UI_STATE_H */
