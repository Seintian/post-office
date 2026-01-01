#ifndef SCREEN_HELP_H
#define SCREEN_HELP_H

#include <stdbool.h>

void tui_InitHelpScreen(void);
void tui_RenderHelpScreen(void);
bool tui_HelpHandleInput(int key);

#endif // SCREEN_HELP_H
