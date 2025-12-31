#ifndef SCREEN_CONFIG_H
#define SCREEN_CONFIG_H

#include <stddef.h>

/**
 * @brief Initializes the Configuration screen state.
 * Loads the configuration file and populates the global state.
 */
void tui_InitConfigScreen(void);

/**
 * @brief Renders the Configuration screen.
 */
void tui_InitConfigScreen(void);
void tui_RenderConfigScreen(void);
void tui_SaveCurrentConfig(void);

#endif // SCREEN_CONFIG_H
