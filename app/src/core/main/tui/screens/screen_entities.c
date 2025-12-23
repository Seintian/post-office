#include "screen_entities.h"
#include <postoffice/tui/tui.h>
#include "../components/entity_table.h"
#include <stdio.h>

tui_widget_t* screen_entities_create(const char* title, int count) {
    tui_rect_t bounds = {0};
    tui_tab_container_t* tabs = tui_tab_container_create(bounds);

    tui_panel_t* p1 = tui_panel_create(bounds, NULL);
    tui_container_set_layout((tui_container_t*)p1, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&p1->base.base.layout_params, 1, 1, 1, 1);

    char buf[64];
    snprintf(buf, sizeof(buf), "%s List (%d Active)", title ? title : "Entity", count);
    tui_label_t* l1 = tui_label_create(buf, (tui_point_t){0,0});
    tui_container_add(&p1->base, (tui_widget_t*)l1);

    // Table
    const char* headers[] = {"ID", "State", "Last Active"};
    tui_widget_t* table = entity_table_create(headers, 3);

    // Dummy Data
    for(int i=0; i<count; i++) {
        char id[16], state[16], time[32];
        snprintf(id, sizeof(id), "%04d", i+1);
        snprintf(state, sizeof(state), i % 2 == 0 ? "Active" : "Idle");
        snprintf(time, sizeof(time), "%d ms ago", i * 100);
        const char* row[] = {id, state, time};
        entity_table_add_row(table, row);
    }
    tui_container_add(&p1->base, table);

    tui_tab_container_add_tab(tabs, title ? title : "Entities", (tui_widget_t*)p1);

    return (tui_widget_t*)tabs;
}
