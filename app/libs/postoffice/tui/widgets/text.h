/**
 * @file text.h
 * @ingroup tui_widgets
 * @brief Multi-line text rendering widget (wrap/scroll planned).
 */

#ifndef POSTOFFICE_TUI_WIDGETS_TEXT_H
#define POSTOFFICE_TUI_WIDGETS_TEXT_H

/** Opaque handle to a multi-line text widget instance. */
typedef struct po_tui_text po_tui_text; /* opaque (internal) */

/**
 * @struct text_widget_config
 * @brief Construction parameters for a text widget.
 */
typedef struct text_widget_config {
    /** Implementation-defined flags (wrapping, scrollbars, etc.). */
    unsigned flags;
} text_widget_config;

/**
 * @brief Create a new text widget.
 * @param cfg Configuration parameters (must not be NULL).
 * @return New instance or NULL on allocation failure.
 */
po_tui_text *text_widget_create(const text_widget_config *cfg);

/**
 * @brief Destroy a text widget and release its resources.
 * @param tw Instance to destroy (may be NULL).
 */
void text_widget_destroy(po_tui_text *tw);

/**
 * @brief Replace the entire text buffer with new UTF‑8 content.
 * @param tw   Target widget (must not be NULL).
 * @param utf8 New UTF‑8 text (owned by caller, copied internally).
 */
void text_widget_set_text(po_tui_text *tw, const char *utf8);

/**
 * @brief Scroll the viewport by a number of rows.
 * @param tw    Target widget (must not be NULL).
 * @param drows Delta rows (positive = scroll down, negative = up).
 */
void text_widget_scroll(po_tui_text *tw, int drows);

#endif /* POSTOFFICE_TUI_WIDGETS_TEXT_H */
