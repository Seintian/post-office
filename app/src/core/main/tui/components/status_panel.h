#ifndef STATUS_PANEL_H
#define STATUS_PANEL_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates a status panel widget.
 * 
 * A status panel displays key-value pairs of information.
 * 
 * @param title The title of the panel.
 * @return Pointer to the created status panel widget.
 */
tui_widget_t* status_panel_create(const char* title);

/**
 * @brief Adds a key-value statistic to the status panel.
 * 
 * @param panel The status panel widget.
 * @param key The label for the statistic.
 * @param value The value of the statistic.
 */
void status_panel_add_stat(tui_widget_t* panel, const char* key, const char* value);

#endif
