#include "screen_dashboard.h"
#include <postoffice/tui/tui.h>
#include "../components/status_panel.h"
#include "../components/entity_table.h"
#include "../components/log_tail_view.h"

tui_widget_t* screen_dashboard_create(void) {
    tui_rect_t bounds = {0};
    tui_tab_container_t* tabs = tui_tab_container_create(bounds);
    
    // Overview Tab
    tui_panel_t* p1 = tui_panel_create(bounds, NULL);
    tui_container_set_layout((tui_container_t*)p1, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&p1->base.base.layout_params, 1, 1, 1, 1);
    
    // Status Panel
    tui_widget_t* sp = status_panel_create("Health Status");
    status_panel_add_stat(sp, "Director", "Running");
    status_panel_add_stat(sp, "Uptime", "00:05:23");
    status_panel_add_stat(sp, "Load", "12%");
    tui_container_add(&p1->base, sp);

    // Logs
    tui_label_t* l_logs = tui_label_create("System Logs:", (tui_point_t){0,0});
    // Add margin
    tui_layout_params_set_margin(&l_logs->base.layout_params, 1, 0, 0, 0);
    tui_container_add(&p1->base, (tui_widget_t*)l_logs);

    tui_widget_t* logs = log_tail_view_create(0); // Fill remaining
    tui_container_add(&p1->base, logs);

    tui_tab_container_add_tab(tabs, "Overview", (tui_widget_t*)p1);

    // Processes Tab
    tui_panel_t* p2 = tui_panel_create(bounds, "Processes");
    tui_container_set_layout((tui_container_t*)p2, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&p2->base.base.layout_params, 1, 1, 1, 1);
    
    const char* headers[] = {"PID", "Name", "Status", "CPU"};
    tui_widget_t* table = entity_table_create(headers, 4);
    
    const char* r1[] = {"1234", "worker-01", "busy", "45%"};
    const char* r2[] = {"1235", "worker-02", "idle", "0%"};
    const char* r3[] = {"1236", "issuer-01", "wait", "2%"};
    entity_table_add_row(table, r1);
    entity_table_add_row(table, r2);
    entity_table_add_row(table, r3);

    tui_container_add(&p2->base, table);
    tui_tab_container_add_tab(tabs, "Processes", (tui_widget_t*)p2);

    return (tui_widget_t*)tabs;
}
