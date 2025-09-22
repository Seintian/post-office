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

/* Internal header: keep types opaque via public API `po_tui_widget` */
typedef struct po_tui_label po_tui_label; /* opaque (internal) */

typedef struct label_config {
    const char *text;
} label_config;
po_tui_label *label_create(const label_config *cfg);
void label_destroy(po_tui_label *lbl);
void label_set_text(po_tui_label *lbl, const char *utf8);

#endif /* POSTOFFICE_TUI_WIDGETS_LABEL_H */
