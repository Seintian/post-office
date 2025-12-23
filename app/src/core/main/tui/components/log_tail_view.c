#include "log_tail_view.h"
#include <postoffice/tui/tui.h>

tui_widget_t* log_tail_view_create(int height) {
    tui_rect_t bounds = {0};
    tui_list_t* list = tui_list_create(bounds);

    // Config layout
    list->base.layout_params.fill_x = true;
    if (height > 0) {
        list->base.layout_params.min_height = height;
    } else {
        list->base.layout_params.expand_y = true;
    }

    tui_list_add_item(list, "[INFO] System started");
    tui_list_add_item(list, "[WARN] Connection latency high");
    tui_list_add_item(list, "[INFO] Director connected");

    return (tui_widget_t*)list;
}

void log_tail_view_add(tui_widget_t* view, const char* msg) {
    tui_list_add_item((tui_list_t*)view, msg);
}
