/**
 * @file table_core.h
 * @ingroup tui_widgets
 * @brief Core model/viewport helpers for table-like widgets backed by callbacks.
 *
 * This header defines the data model contract for table widgets: callers supply
 * callbacks to enumerate row/column counts and provide cell strings on demand.
 * The core maintains a viewport/scroller and selection coordinates.
 */

#ifndef POSTOFFICE_TUI_WIDGETS_TABLE_CORE_H
#define POSTOFFICE_TUI_WIDGETS_TABLE_CORE_H

/** Opaque handle to the table core instance. */
typedef struct po_tui_table_core po_tui_table_core; /* opaque (internal) */

/** Callback type returning the number of rows available. */
typedef int (*table_get_rows)(void *ud);

/** Callback type returning the number of columns available. */
typedef int (*table_get_cols)(void *ud);

/**
 * Callback type returning the textual contents of a cell as UTF‑8.
 * @param ud  User data provided by the model owner.
 * @param row Row index in [0, rows).
 * @param col Column index in [0, cols).
 * @return Pointer to a stable string for the requested cell; may be NULL to
 *         render as empty.
 */
typedef const char *(*table_get_cell)(void *ud, int row, int col);

/**
 * @struct table_model
 * @brief Data source for a table; supplies sizes and cell text lazily.
 *
 * All callbacks must be valid function pointers; `ud` is passed back verbatim
 * to each callback and can be used to reference user-owned state.
 */
typedef struct table_model {
    table_get_rows get_rows; /**< Returns number of rows (>= 0). */
    table_get_cols get_cols; /**< Returns number of columns (>= 0). */
    table_get_cell get_cell; /**< Returns cell text for (row,col) or NULL. */
    void *ud;                /**< User data forwarded to callbacks. */
} table_model;

/**
 * @brief Create a table core bound to the given model.
 * @param model Non-NULL model with valid callbacks.
 * @return New instance or NULL on allocation failure.
 */
po_tui_table_core *table_core_create(const table_model *model);
/**
 * @brief Destroy a table core instance and release resources.
 * @param t Instance to destroy (may be NULL).
 */
void table_core_destroy(po_tui_table_core *t);
/**
 * @brief Replace the data model used by the table core.
 * @param t     Table core (must not be NULL).
 * @param model New model (must not be NULL).
 */
void table_core_set_model(po_tui_table_core *t, const table_model *model);
/**
 * @brief Scroll the viewport by the given deltas.
 * @param t     Table core (must not be NULL).
 * @param drows Delta in rows (positive to scroll down, negative up).
 * @param dcols Delta in columns (positive to scroll right, negative left).
 */
void table_core_scroll(po_tui_table_core *t, int drows, int dcols);
/**
 * @brief Set the selection cell in model coordinates.
 * @param t   Table core (must not be NULL).
 * @param row Row index in [0, rows) or -1 to clear selection.
 * @param col Column index in [0, cols) or -1 to clear selection.
 */
void table_core_set_selection(po_tui_table_core *t, int row, int col);

#endif /* POSTOFFICE_TUI_WIDGETS_TABLE_CORE_H */
