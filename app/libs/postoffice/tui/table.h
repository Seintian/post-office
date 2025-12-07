#ifndef TUI_TABLE_H
#define TUI_TABLE_H

#include <tui/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Table column definition
typedef struct {
    char* title;
    int width;
    float weight; // For auto-sizing (0 = fixed width)
} tui_table_column_t;

// Table widget structure
typedef struct tui_table_t {
    tui_widget_t base;
    
    tui_table_column_t* columns;
    int column_count;
    
    char*** rows; // Array of string arrays (row -> col)
    int row_count;
    int row_capacity;
    
    int selected_row;
    int top_visible_row;
    int visible_rows;
    
    bool show_headers;
    bool show_grid;
    
    // Callbacks
    void (*on_select)(struct tui_table_t* table, int row, void* user_data);
} tui_table_t;

/**
 * @brief Create a new table widget
 * @param bounds Position and size
 * @return New table widget
 */
tui_table_t* tui_table_create(tui_rect_t bounds);

/**
 * @brief Add a column to the table
 * @param table Table widget
 * @param title Column title
 * @param width Fixed width (or 0 for auto/weighted)
 * @param weight Weight for extra space distribution
 */
void tui_table_add_column(tui_table_t* table, const char* title, int width, float weight);

/**
 * @brief Add a row of data
 * @param table Table widget
 * @param cell_data Array of strings for each column (must match column count)
 */
void tui_table_add_row(tui_table_t* table, const char** cell_data);

/**
 * @brief Clear all rows
 */
void tui_table_clear(tui_table_t* table);

/**
 * @brief Set selection callback
 */
void tui_table_set_select_callback(tui_table_t* table, void (*callback)(tui_table_t*, int, void*), void* user_data);

#ifdef __cplusplus
}
#endif

#endif // TUI_TABLE_H
