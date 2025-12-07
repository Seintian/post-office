#include "screen_template.h"
#include <postoffice/tui/tui.h>

// Using screen_template as the "User" screen as per previous analysis
tui_widget_t* screen_user_create(void) {
    tui_rect_t bounds = {0};
    tui_tab_container_t* tabs = tui_tab_container_create(bounds);
    
    tui_panel_t* p1 = tui_panel_create(bounds, "Request Form");
    tui_container_set_layout((tui_container_t*)p1, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&p1->base.base.layout_params, 2, 1, 2, 1); 

    tui_container_add(&p1->base, (tui_widget_t*)tui_label_create("Enter Request:", (tui_point_t){0,0}));
    
    tui_input_field_t* inp = tui_input_field_create(bounds, 50);
    inp->base.layout_params.fill_x = true;
    inp->base.layout_params.min_height = 1;
    tui_layout_params_set_margin(&inp->base.layout_params, 0, 1, 0, 0); 
    tui_container_add(&p1->base, (tui_widget_t*)inp);
    
    tui_tab_container_add_tab(tabs, "New Request", (tui_widget_t*)p1);

    return (tui_widget_t*)tabs;
}
