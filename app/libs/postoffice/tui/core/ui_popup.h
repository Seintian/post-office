/**
 * @file ui_popup.h
 * @ingroup tui_core
 * @brief WHAT: Modal popup composition primitives.
 */

#ifndef POSTOFFICE_TUI_CORE_UI_POPUP_H
#define POSTOFFICE_TUI_CORE_UI_POPUP_H

struct ui_popup;   /* opaque */
struct ui_context; /* fwd */

struct ui_popup *ui_popup_create(struct ui_context *ctx);
void ui_popup_destroy(struct ui_popup *p);
int ui_popup_show(struct ui_popup *p);
int ui_popup_hide(struct ui_popup *p);

/* Set popup content; ownership of root stays with caller unless specified */
void ui_popup_set_content(struct ui_popup *p, struct po_tui_widget *root);
int ui_popup_is_visible(const struct ui_popup *p);

#endif /* POSTOFFICE_TUI_CORE_UI_POPUP_H */
