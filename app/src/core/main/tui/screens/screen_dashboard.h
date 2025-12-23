#ifndef SCREEN_DASHBOARD_H
#define SCREEN_DASHBOARD_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates the main Dashboard screen.
 * 
 * Displays an overview of system health, running processes, and recent logs.
 * 
 * @return Pointer to the dashboard screen widget.
 */
tui_widget_t* screen_dashboard_create(void);

#endif
