#include "entity_table.h"
#include <postoffice/tui/tui.h>
#include <stdlib.h>

tui_widget_t* entity_table_create(const char** headers, int col_count) {
    tui_rect_t bounds = {0};
    tui_table_t* table = tui_table_create(bounds);
    table->base.layout_params.expand_y = true;
    table->base.layout_params.fill_x = true;

    for(int i=0; i<col_count; i++) {
        tui_table_add_column(table, headers[i], 15, 1.0f);
    }
    
    return (tui_widget_t*)table;
}

void entity_table_add_row(tui_widget_t* table, const char** row_data) {
    tui_table_add_row((tui_table_t*)table, row_data);
}
