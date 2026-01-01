#ifndef SCREEN_ENTITIES_H
#define SCREEN_ENTITIES_H

void tui_InitEntities(void);
void tui_UpdateEntities(void);
void tui_RenderEntitiesScreen(void);
void tui_RenderEntitiesScreen(void);
void tui_EntitiesHandleInput(int key); // Handle scrolling/filtering
void tui_UpdateEntitiesFilter(void);

#endif // SCREEN_ENTITIES_H
