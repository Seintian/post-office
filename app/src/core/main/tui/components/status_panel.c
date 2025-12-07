#include "status_panel.h"
#include <postoffice/tui/tui.h>
#include <stdio.h>

tui_widget_t* status_panel_create(const char* title) {
    tui_rect_t bounds = {0};
    tui_panel_t* panel = tui_panel_create(bounds, title);
    tui_container_set_layout((tui_container_t*)panel, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&panel->base.base.layout_params, 1, 1, 1, 1);
    return (tui_widget_t*)panel;
}

void status_panel_add_stat(tui_widget_t* panel, const char* key, const char* value) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s: %s", key, value);
    tui_label_t* label = tui_label_create(buf, (tui_point_t){0,0});
    tui_container_add((tui_container_t*)panel, (tui_widget_t*)label);
}
