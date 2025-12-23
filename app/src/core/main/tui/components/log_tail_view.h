#ifndef LOG_TAIL_VIEW_H
#define LOG_TAIL_VIEW_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates a log tail view widget.
 * 
 * Displays a scrolling list of log messages.
 * 
 * @param height Fixed height for the view, or 0 to expand.
 * @return Pointer to the created log view widget.
 */
tui_widget_t* log_tail_view_create(int height);

/**
 * @brief Adds a message to the log tail view.
 * 
 * @param view The log view widget.
 * @param msg The message string to add.
 */
void log_tail_view_add(tui_widget_t* view, const char* msg);

#endif
