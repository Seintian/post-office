#ifndef SCREEN_TEMPLATE_H
#define SCREEN_TEMPLATE_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates the User Interaction screen.
 * 
 * This screen allows manual creation of user requests (repurposed from template).
 * 
 * @return Pointer to the user screen widget.
 */
tui_widget_t* screen_user_create(void);

#endif
