#ifndef SCREEN_PERFORMANCE_H
#define SCREEN_PERFORMANCE_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates the performance monitoring screen.
 * 
 * Displays system performance metrics such as CPU usage and memory consumption
 * using graphs and gauges.
 * 
 * @return Pointer to the performance screen widget.
 */
tui_widget_t* screen_performance_create(void);

#endif
