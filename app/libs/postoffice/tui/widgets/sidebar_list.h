/**
 * @file sidebar_list.h
 * @ingroup tui_widgets
 * @brief Vertical list widget suited for sidebar navigation with single selection.
 *
 * The sidebar list displays a stack of textual items and tracks a single
 * selected row. Items can be added or cleared via API, and the selected index
 * can be queried or changed programmatically.
 */

#ifndef POSTOFFICE_TUI_WIDGETS_SIDEBAR_LIST_H
#define POSTOFFICE_TUI_WIDGETS_SIDEBAR_LIST_H

/** Opaque handle to a sidebar list widget instance. */
typedef struct po_tui_sidebar_list po_tui_sidebar_list; /* opaque (internal) */

/**
 * @struct sidebar_list_config
 * @brief Construction parameters for a sidebar list.
 */
typedef struct sidebar_list_config {
    /** Behavior flags (currently none defined; pass 0). */
    unsigned flags;
} sidebar_list_config;

/**
 * @brief Create a new sidebar list widget.
 * @param cfg Optional configuration; if NULL, defaults are used (flags=0).
 * @return New instance on success; NULL on allocation failure.
 */
po_tui_sidebar_list *sidebar_list_create(const sidebar_list_config *cfg);
/**
 * @brief Destroy a sidebar list and free its resources.
 * @param sl Instance to destroy (may be NULL).
 */
void sidebar_list_destroy(po_tui_sidebar_list *sl);
/**
 * @brief Append a new item with the given label (UTF‑8).
 * @param sl         Target list (must not be NULL).
 * @param utf8_label Item text (must not be NULL; empty allowed).
 * @return The 0-based index of the inserted item on success, or -1 on failure.
 */
int sidebar_list_add(po_tui_sidebar_list *sl, const char *utf8_label);
/**
 * @brief Remove all items from the list and clear the selection.
 * @param sl Target list (must not be NULL).
 */
void sidebar_list_clear(po_tui_sidebar_list *sl);
/**
 * @brief Get the number of items currently present.
 * @param sl Target list (must not be NULL).
 * @return Items count (>= 0).
 */
int sidebar_list_count(const po_tui_sidebar_list *sl);
/**
 * @brief Get the index of the selected item.
 * @param sl Target list (must not be NULL).
 * @return Selected index in [0, count) or -1 if none.
 */
int sidebar_list_selected(const po_tui_sidebar_list *sl);
/**
 * @brief Set the selected item index.
 * @param sl  Target list (must not be NULL).
 * @param idx Index in [0, count) to select, or -1 to clear selection. Out-of-range values are ignored.
 */
void sidebar_list_set_selected(po_tui_sidebar_list *sl, int idx);

#endif /* POSTOFFICE_TUI_WIDGETS_SIDEBAR_LIST_H */
