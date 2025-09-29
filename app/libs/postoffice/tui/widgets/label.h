/**
 * @file label.h
 * @ingroup tui_widgets
 * @brief Read-only text label widget.
 *
 * Renders a single line of text without wrapping. Style and alignment are
 * applied by the theme/cascade when available.
 */

#ifndef POSTOFFICE_TUI_WIDGETS_LABEL_H
#define POSTOFFICE_TUI_WIDGETS_LABEL_H

/** Opaque handle to a label widget instance. */
typedef struct po_tui_label po_tui_label; /* opaque (internal) */

/**
 * @struct label_config
 * @brief Construction parameters for a label widget.
 */
typedef struct label_config {
    /** Initial UTF-8 text to display. */
    const char *text;
} label_config;

/**
 * @brief Create a new label widget.
 * @param cfg Configuration parameters (must not be NULL).
 * @return New label instance or NULL on allocation failure.
 */
po_tui_label *label_create(const label_config *cfg);

/**
 * @brief Destroy a label widget and release its resources.
 * @param lbl Label instance to destroy (may be NULL).
 */
void label_destroy(po_tui_label *lbl);

/**
 * @brief Replace the label's text content.
 * @param lbl  Target label (must not be NULL).
 * @param utf8 New UTF-8 text (owned by caller, copied internally).
 */
void label_set_text(po_tui_label *lbl, const char *utf8);

#endif /* POSTOFFICE_TUI_WIDGETS_LABEL_H */
