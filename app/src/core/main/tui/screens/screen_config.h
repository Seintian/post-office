#ifndef SCREEN_CONFIG_H
#define SCREEN_CONFIG_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates the configuration editor screen.
 * 
 * Allows viewing and (in the future) editing of loaded configuration values.
 * 
 * @return A new widget representing the configuration screen.
 */
tui_widget_t* screen_config_create(void);



/**

 * @brief Set the path to the configuration file to be edited.

 * @param path Path string (copied internally).

 */





#endif // SCREEN_CONFIG_H
