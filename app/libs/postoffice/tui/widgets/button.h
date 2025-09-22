/**
 * @file button.h
 * @ingroup tui_widgets
 * @brief Activatable button widget with focus/press states.
 */

#ifndef POSTOFFICE_TUI_WIDGETS_BUTTON_H
#define POSTOFFICE_TUI_WIDGETS_BUTTON_H

typedef struct po_tui_button po_tui_button; /* opaque (internal) */

typedef void (*button_on_click)(void *ud);
typedef struct button_config {
    const char *text;
    button_on_click on_click;
    void *on_click_ud;
} button_config;

po_tui_button *button_create(const button_config *cfg);
void button_destroy(po_tui_button *btn);
void button_set_text(po_tui_button *btn, const char *utf8);
void button_set_on_click(po_tui_button *btn, button_on_click cb, void *ud);

#endif /* POSTOFFICE_TUI_WIDGETS_BUTTON_H */
