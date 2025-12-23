#ifndef TOPBAR_H
#define TOPBAR_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates the top status bar widget.
 * 
 * The top bar contains the application title and a dynamic status label.
 * 
 * @return Pointer to the created top bar widget (as tui_widget_t*).
 */
tui_widget_t* topbar_create(void);

/**
 * @brief Updates the status text in the top bar.
 * 
 * @param topbar The top bar widget.
 * @param status The new status text to display.
 */
void topbar_set_status(tui_widget_t* topbar, const char* status);

#endif
