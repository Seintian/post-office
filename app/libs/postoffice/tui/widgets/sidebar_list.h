/**
 * @file sidebar_list.h
 * @ingroup tui_widgets
 * @brief WHAT: Vertical list widget for sidebars with selection.
 */

#ifndef POSTOFFICE_TUI_WIDGETS_SIDEBAR_LIST_H
#define POSTOFFICE_TUI_WIDGETS_SIDEBAR_LIST_H

typedef struct po_tui_sidebar_list po_tui_sidebar_list; /* opaque (internal) */

typedef struct sidebar_list_config {
    unsigned flags;
} sidebar_list_config;

po_tui_sidebar_list *sidebar_list_create(const sidebar_list_config *cfg);
void sidebar_list_destroy(po_tui_sidebar_list *sl);
int sidebar_list_add(po_tui_sidebar_list *sl, const char *utf8_label);
void sidebar_list_clear(po_tui_sidebar_list *sl);
int sidebar_list_count(const po_tui_sidebar_list *sl);
int sidebar_list_selected(const po_tui_sidebar_list *sl);
void sidebar_list_set_selected(po_tui_sidebar_list *sl, int idx);

#endif /* POSTOFFICE_TUI_WIDGETS_SIDEBAR_LIST_H */
