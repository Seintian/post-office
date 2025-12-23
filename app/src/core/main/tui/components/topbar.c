#include "topbar.h"
#include <postoffice/tui/tui.h>
#include <string.h>
#include <stdlib.h>

/* 
    We need to know the structure of tui_label_t to update text safely.
    Assuming standard PostOffice TUI struct layout where text is a char*.
*/

tui_widget_t* topbar_create(void) {
    tui_rect_t bounds = {0}; 
    tui_panel_t* panel = tui_panel_create(bounds, NULL);
    panel->show_border = true;

    tui_container_set_layout((tui_container_t*)panel, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0));

    // Title
    tui_label_t* title = tui_label_create("Post Office Simulation", (tui_point_t){0,0});
    title->base.layout_params.h_align = TUI_ALIGN_CENTER;
    title->base.layout_params.fill_x = true;
    tui_container_add(&panel->base, (tui_widget_t*)title);

    // Status
    tui_label_t* status = tui_label_create("Status: Initializing...", (tui_point_t){0,0});
    status->base.layout_params.h_align = TUI_ALIGN_CENTER;
    status->base.layout_params.fill_x = true;
    tui_container_add(&panel->base, (tui_widget_t*)status);

    // Store status label in user_data for retrieval
    panel->base.base.user_data = status;

    return (tui_widget_t*)panel;
}

void topbar_set_status(tui_widget_t* topbar, const char* status_text) {
    if (!topbar) return;
    tui_label_t* label = (tui_label_t*)topbar->user_data;
    if (label && status_text) {
        if(label->text) free(label->text);
        label->text = strdup(status_text);
    }
}
