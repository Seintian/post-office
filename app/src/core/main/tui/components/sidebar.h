#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates the sidebar navigation widget.
 * 
 * The sidebar contains a list of selectable items corresponding to different application views.
 * 
 * @param on_select Callback function to execute when a list item is selected.
 * @param userdata User data passed to the callback.
 * @param out_list Optional output pointer to the created tui_list_t widget (for manual selection control).
 * @return Pointer to the created sidebar widget (as tui_widget_t*).
 */
tui_widget_t* sidebar_create(tui_list_select_callback_t on_select, void* userdata, tui_list_t** out_list);

#endif
