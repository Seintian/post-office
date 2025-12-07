#ifndef ENTITY_TABLE_H
#define ENTITY_TABLE_H

#include <postoffice/tui/tui.h>

tui_widget_t* entity_table_create(const char** headers, int col_count);
void entity_table_add_row(tui_widget_t* table, const char** row_data);

#endif
