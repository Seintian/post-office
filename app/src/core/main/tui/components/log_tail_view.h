#ifndef LOG_TAIL_VIEW_H
#define LOG_TAIL_VIEW_H

#include <postoffice/tui/tui.h>

tui_widget_t* log_tail_view_create(int height);
void log_tail_view_add(tui_widget_t* view, const char* msg);

#endif
