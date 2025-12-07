#ifndef STATUS_PANEL_H
#define STATUS_PANEL_H

#include <postoffice/tui/tui.h>

tui_widget_t* status_panel_create(const char* title);
void status_panel_add_stat(tui_widget_t* panel, const char* key, const char* value);

#endif
