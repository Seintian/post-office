#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <postoffice/tui/tui.h>

tui_widget_t* sidebar_create(tui_list_select_callback_t on_select, void* userdata);

#endif
