#include "command_field.h"
#include <postoffice/tui/tui.h>

tui_widget_t* command_field_create(void) {
    tui_rect_t bounds = {0};
    tui_panel_t* panel = tui_panel_create(bounds, "Command");
    
    tui_container_set_layout((tui_container_t*)panel, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0));
    tui_layout_params_set_padding(&panel->base.base.layout_params, 1, 0, 1, 0);

    tui_input_field_t* input = tui_input_field_create(bounds, 128);
    input->base.layout_params.fill_x = true;
    input->base.layout_params.min_height = 1;
    tui_layout_params_set_margin(&input->base.layout_params, 1, 0, 0, 0);

    tui_container_add(&panel->base, (tui_widget_t*)input);
    
    return (tui_widget_t*)panel;
}
