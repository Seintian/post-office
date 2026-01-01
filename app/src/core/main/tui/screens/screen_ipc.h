#ifndef SCREEN_IPC_H
#define SCREEN_IPC_H

#include <stdbool.h>

void tui_InitIPCScreen(void);
void tui_UpdateIPCScreen(void);
void tui_RenderIPCScreen(void);
bool tui_IPCHandleInput(int key);

#endif // SCREEN_IPC_H
