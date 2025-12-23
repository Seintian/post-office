#ifndef COMMAND_FIELD_H
#define COMMAND_FIELD_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates the command input field widget.
 * 
 * The command field allows the user to type and execute commands.
 * 
 * @return Pointer to the created command field widget (as tui_widget_t*).
 */
tui_widget_t* command_field_create(void);

#endif
