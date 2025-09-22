/**
 * @file widgets.h
 * @brief Public widget creation and composition API for Post Office TUI.
 *
 * All widget types are opaque. Users compose trees by creating container
 * widgets (flex, grid, stack, box) and adding child widgets.
 */

#ifndef POSTOFFICE_TUI_WIDGETS_API_H
#define POSTOFFICE_TUI_WIDGETS_API_H

#include <stddef.h>

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque types are declared in types.h */

/*
 * Generic widget API
 */

int po_tui_widget_set_id(po_tui_widget *w, const char *id);
int po_tui_widget_set_visible(po_tui_widget *w, int visible);
int po_tui_widget_set_enabled(po_tui_widget *w, int enabled);

/* Container composition */
int po_tui_container_add_child(po_tui_widget *container, po_tui_widget *child);
int po_tui_container_remove_child(po_tui_widget *container, po_tui_widget *child);

/*
 * Leaf widgets
 */

po_tui_widget *po_tui_label_create(po_tui_app *app, const char *text);
int po_tui_label_set_text(po_tui_widget *w, const char *text);

po_tui_widget *po_tui_button_create(po_tui_app *app, const char *label);
int po_tui_button_set_text(po_tui_widget *w, const char *label);

po_tui_widget *po_tui_progress_create(po_tui_app *app);
int po_tui_progress_set_fraction(po_tui_widget *w, float fraction_0_1);

po_tui_widget *po_tui_text_create(po_tui_app *app);
int po_tui_text_set_content(po_tui_widget *w, const char *utf8_text);

po_tui_widget *po_tui_sidebar_list_create(po_tui_app *app);
int po_tui_sidebar_list_set_items(po_tui_widget *w, const char *const *items, size_t count);

po_tui_widget *po_tui_table_create(po_tui_app *app);
/* Future: table model hookup functions */

/*
 * Container widgets (also widgets): flex, grid, stack, box
 */

/* Box */
po_tui_widget *po_tui_box_create(po_tui_app *app, const char *title);

/* Flex */
typedef enum po_tui_flex_dir { PO_TUI_ROW = 0, PO_TUI_COLUMN = 1 } po_tui_flex_dir;
po_tui_widget *po_tui_flex_create(po_tui_app *app, po_tui_flex_dir dir, int gap);
int po_tui_flex_set_direction(po_tui_widget *flex, po_tui_flex_dir dir);
int po_tui_flex_set_gap(po_tui_widget *flex, int gap);
int po_tui_flex_set_child_grow(po_tui_widget *child, float grow);
int po_tui_flex_set_child_shrink(po_tui_widget *child, float shrink);

/* Grid */
po_tui_widget *po_tui_grid_create(po_tui_app *app, int rows, int cols);
int po_tui_grid_set_gaps(po_tui_widget *grid, int row_gap, int col_gap);
int po_tui_grid_place(po_tui_widget *grid, po_tui_widget *child, int row, int col, int rowspan,
                      int colspan);

/* Stack */
po_tui_widget *po_tui_stack_create(po_tui_app *app);
int po_tui_stack_push(po_tui_widget *stack, po_tui_widget *child);
int po_tui_stack_pop(po_tui_widget *stack, po_tui_widget *child);

#ifdef __cplusplus
}
#endif

#endif /* POSTOFFICE_TUI_WIDGETS_API_H */
