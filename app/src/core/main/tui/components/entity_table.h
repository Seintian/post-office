#ifndef ENTITY_TABLE_H
#define ENTITY_TABLE_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates an entity table widget.
 * 
 * @param headers Array of column header strings.
 * @param col_count Number of columns.
 * @return Pointer to the created table widget.
 */
tui_widget_t* entity_table_create(const char** headers, int col_count);

/**
 * @brief Adds a row of data to the entity table.
 * 
 * @param table The table widget.
 * @param row_data Array of strings representing the row values.
 */
void entity_table_add_row(tui_widget_t* table, const char** row_data);

#endif
