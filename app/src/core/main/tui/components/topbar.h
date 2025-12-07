#ifndef TOPBAR_H
#define TOPBAR_H

#include <postoffice/tui/tui.h>

tui_widget_t* topbar_create(void);
void topbar_set_status(tui_widget_t* topbar, const char* status);

#endif
