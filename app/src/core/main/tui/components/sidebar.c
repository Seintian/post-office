#include "sidebar.h"
#include <postoffice/tui/tui.h>
#include <stdlib.h>

tui_widget_t* sidebar_create(tui_list_select_callback_t on_select, void* userdata, tui_list_t** out_list) {
    tui_rect_t bounds = {0};
    tui_panel_t* panel = tui_panel_create(bounds, "Menu");

    tui_container_set_layout((tui_container_t*)panel, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0));
    tui_layout_params_set_padding(&panel->base.base.layout_params, 1, 1, 1, 1);

    tui_list_t* list = tui_list_create(bounds);
    list->base.layout_params.expand_y = true;
    list->base.layout_params.fill_x = true;
    list->base.layout_params.weight_y = 1.0f;

    tui_list_add_item(list, "Director");
    tui_list_add_item(list, "Ticket Issuer");
    tui_list_add_item(list, "Users Manager");
    tui_list_add_item(list, "Worker");
    tui_list_add_item(list, "User");
    tui_list_add_item(list, "Performance");
    tui_list_add_item(list, "Configuration");

    if (on_select) {
        tui_list_set_select_callback(list, on_select, userdata);
    }

    if (out_list) {
        *out_list = list;
    }

    tui_container_add(&panel->base, (tui_widget_t*)list);

    return (tui_widget_t*)panel;
}
