/**
 * @file table_core.h
 * @ingroup tui_widgets
 * @brief WHAT: Core table model and rendering for large datasets.
 */

#ifndef POSTOFFICE_TUI_WIDGETS_TABLE_CORE_H
#define POSTOFFICE_TUI_WIDGETS_TABLE_CORE_H

typedef struct po_tui_table_core po_tui_table_core; /* opaque (internal) */

typedef int (*table_get_rows)(void *ud);
typedef int (*table_get_cols)(void *ud);
typedef const char *(*table_get_cell)(void *ud, int row, int col);

typedef struct table_model {
    table_get_rows get_rows;
    table_get_cols get_cols;
    table_get_cell get_cell;
    void *ud;
} table_model;

po_tui_table_core *table_core_create(const table_model *model);
void table_core_destroy(po_tui_table_core *t);
void table_core_set_model(po_tui_table_core *t, const table_model *model);
void table_core_scroll(po_tui_table_core *t, int drows, int dcols);
void table_core_set_selection(po_tui_table_core *t, int row, int col);

#endif /* POSTOFFICE_TUI_WIDGETS_TABLE_CORE_H */
