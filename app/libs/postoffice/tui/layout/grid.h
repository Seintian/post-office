/**
 * @file grid.h
 * @ingroup tui_layout
 * @brief Grid layout constraints and API.
 */

#ifndef POSTOFFICE_TUI_LAYOUT_GRID_H
#define POSTOFFICE_TUI_LAYOUT_GRID_H

typedef struct po_tui_grid po_tui_grid; /* opaque (internal) */

typedef enum grid_size_mode { GRID_AUTO, GRID_FIXED, GRID_FRACTION } grid_size_mode;

typedef struct grid_track_spec {
    grid_size_mode mode;
    int value; /* cells for FIXED, fraction units for FRACTION */
} grid_track_spec;

typedef struct grid_config {
    const grid_track_spec *cols;
    int cols_count;
    const grid_track_spec *rows;
    int rows_count;
    int gap_col;
    int gap_row;
} grid_config;

po_tui_grid *grid_create(const grid_config *cfg);
void grid_destroy(po_tui_grid *g);
void grid_set_config(po_tui_grid *g, const grid_config *cfg);

#endif /* POSTOFFICE_TUI_LAYOUT_GRID_H */
