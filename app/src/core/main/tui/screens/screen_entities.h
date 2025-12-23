#ifndef SCREEN_ENTITIES_H
#define SCREEN_ENTITIES_H

#include <postoffice/tui/tui.h>

/**
 * @brief Creates a generic entity list screen.
 * 
 * Displays a list of entities (e.g., Workers, Users) in a table format.
 * 
 * @param title Screen title.
 * @param count Number of entities to simulate (dummy data).
 * @return Pointer to the entities screen widget.
 */
tui_widget_t* screen_entities_create(const char* title, int count);

#endif
