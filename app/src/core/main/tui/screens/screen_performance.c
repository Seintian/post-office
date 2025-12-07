#include "screen_performance.h"
#include <postoffice/tui/tui.h>
#include <math.h>

tui_widget_t* screen_performance_create(void) {
    tui_rect_t bounds = {0};
    tui_tab_container_t* tabs = tui_tab_container_create(bounds);
    
    tui_panel_t* p1 = tui_panel_create(bounds, "CPU & Memory");
    tui_container_set_layout((tui_container_t*)p1, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&p1->base.base.layout_params, 1, 1, 1, 1);

    // CPU Graph
    tui_label_t* l1 = tui_label_create("CPU Usage History:", (tui_point_t){0,0});
    tui_container_add(&p1->base, (tui_widget_t*)l1);
    
    tui_rect_t graph_r = {0}; // Layout will resize
    tui_graph_t* graph = tui_graph_create(graph_r);
    graph->base.layout_params.fill_x = true;
    graph->base.layout_params.min_height = 8;
    
    // Add some random data
    for(int i=0; i<20; i++) {
        tui_graph_add_value(graph, 20.0f + (float)(i*2));
    }
    tui_container_add(&p1->base, (tui_widget_t*)graph);

    // Memory Gauge
    tui_label_t* l2 = tui_label_create("Memory Usage:", (tui_point_t){0,0});
    tui_layout_params_set_margin(&l2->base.layout_params, 1, 0, 0, 0);
    tui_container_add(&p1->base, (tui_widget_t*)l2);

    tui_rect_t gauge_r = {0};
    tui_gauge_t* gauge = tui_gauge_create(gauge_r, 100.0f);
    tui_gauge_set_label(gauge, "1024 MB");
    tui_gauge_set_value(gauge, 45.0f);
    gauge->base.layout_params.fill_x = true;
    gauge->base.layout_params.min_height = 3;
    tui_container_add(&p1->base, (tui_widget_t*)gauge);

    tui_tab_container_add_tab(tabs, "Metrics", (tui_widget_t*)p1);

    return (tui_widget_t*)tabs;
}
