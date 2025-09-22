/**
 * @file text.h
 * @ingroup tui_widgets
 * @brief WHAT: Multi-line text rendering widget (wrap/scroll planned).
 */

#ifndef POSTOFFICE_TUI_WIDGETS_TEXT_H
#define POSTOFFICE_TUI_WIDGETS_TEXT_H

typedef struct po_tui_text po_tui_text; /* opaque (internal) */

typedef struct text_widget_config {
    unsigned flags;
} text_widget_config;

po_tui_text *text_widget_create(const text_widget_config *cfg);
void text_widget_destroy(po_tui_text *tw);
void text_widget_set_text(po_tui_text *tw, const char *utf8);
void text_widget_scroll(po_tui_text *tw, int drows);

#endif /* POSTOFFICE_TUI_WIDGETS_TEXT_H */
