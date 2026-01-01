#include "screen_entities.h"
#include "../tui_state.h"
#include "../components/data_table.h"
#include <clay/clay.h>
#include <renderer/clay_ncurses_renderer.h>
#include <stdio.h>

// Extern definitions for adapter
extern const DataTableAdapter g_EntitiesAdapter;
extern void tui_InitEntities(void); // Prototype

// Definition of columns
static const DataTableDef g_EntitiesTableDef = {
    .columns = {
        {0, "ID", 6.0f, true},
        {1, "Type", 12.0f, true},
        {2, "Name", 20.0f, true},
        {3, "State", 12.0f, true},
        {4, "Location", 20.0f, true},
        {5, "Q", 6.0f, true},
        {6, "CPU", 8.0f, true}
    },
    .columnCount = 7,
    .adapter = {
        // We link directly to the adapter implementation 
        // (wrapping might be needed if C complains about non-constant structs in initialization)
        // But since we declared g_EntitiesAdapter as const, we can copy its fields or point to it?
        // DataTableDef holds a COPY of the adapter struct, so this is fine.
        .GetCount = NULL, // Filled in Init/Runtime? Or we need to expose the functions.
        // Actually, let's just use externs for the functions in adapter_entities.c if we want static init,
        // or helper getter.
        // Simplified: The adapter in `adapter_entities.c` is available via `g_EntitiesAdapter`.
    }
};

// We need to copy the function pointers because C doesn't support struct inheritance or easy static mixin.
// Let's resolve this by taking the adapter from the global symbol at runtime
// OR declaring the functions in a header.
// For now, let's just "patch" the def in Render or use a helper.

// Forward decls from adapter_entities.c (or make them public in header? No header for adapter yet).
// Let's copy the adapter struct entirely at runtime.

static DataTableDef g_ActiveTableDef; 
static bool g_TableInitialized = false;

static void InitTableDef(void) {
    g_ActiveTableDef = g_EntitiesTableDef;
    g_ActiveTableDef.adapter = g_EntitiesAdapter;
    g_TableInitialized = true;
}

static void CloseModal(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
    (void)elementId; (void)pointerData; (void)userData;
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        g_tuiState.selectedEntityIndex = -1;
    }
}

static void RenderEntityDetailModal(void) {
    if (g_tuiState.selectedEntityIndex == -1) return;

    MockEntity *e = &g_tuiState.mockEntities[g_tuiState.selectedEntityIndex];

    CLAY(CLAY_ID("ModalCenter"),
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .padding = {10 * TUI_CW, 10 * TUI_CW, 5 * TUI_CH, 5 * TUI_CH}}}) {
         
         // The box itself
         CLAY(CLAY_ID("ModalBox"),
            {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .padding = {2 * TUI_CW, 2 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH},
                        .childGap = 1 * TUI_CH},
             .backgroundColor = {20, 20, 20, 255},
             .border = {.width = {2,2,2,2,0}, .color = COLOR_ACCENT}}) {
                 
                 // Header
                 CLAY(CLAY_ID("ModalHeader"), 
                    {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()},
                                .layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = 2 * TUI_CW}}) {
                     
                     CLAY_TEXT(CLAY_STRING_DYN(e->name), CLAY_TEXT_CONFIG({.fontId = CLAY_NCURSES_FONT_BOLD, .fontSize = 16, .textColor = COLOR_ACCENT}));
                     
                     CLAY(CLAY_ID("ModalHeaderSpacer"), {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(TUI_CH)}}});

                     CLAY_TEXT(CLAY_STRING("X"), CLAY_TEXT_CONFIG({.textColor = {200, 50, 50, 255}}));
                     Clay_Ncurses_OnClick(CloseModal, NULL);
                 }
                 
                 // Details
                 CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("ID: %u", e->id)), CLAY_TEXT_CONFIG({.textColor = {200,200,200,255}}));

                 CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("Location: %s", e->location)), CLAY_TEXT_CONFIG({.textColor = {200,200,200,255}}));
                 
                 CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("State: %s", e->state)), CLAY_TEXT_CONFIG({.textColor = {200,200,200,255}}));
                 
                 CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("CPU Usage: %.2f%%", (double)e->cpuUsage)), CLAY_TEXT_CONFIG({.textColor = {200,200,200,255}}));
            }
    }
}


void OnEntityTabClick(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
    (void)elementId;
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        g_tuiState.activeEntitiesTab = (uint32_t)(intptr_t)userData;
    }
}

static void OnTabClick(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
    (void)elementId;
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        g_tuiState.activeEntitiesTab = (uint32_t)(intptr_t)userData;
        tui_UpdateEntitiesFilter(); // Refresh data!
    }
}



void tui_RenderEntitiesScreen(void) {
    if (!g_TableInitialized) InitTableDef();

    // -- Top Bar (Tabs + Filter) --
    CLAY(CLAY_ID("EntitiesTop"), 
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(3 * TUI_CH)}, // Exact fit (1 text + 2 borders)
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .childGap = 2 * TUI_CW,
                    .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}, // Center Y to alignment Filter
                    .padding = {0, 1 * TUI_CW, 0, 0}}}) { // Left padding 0 for alignment

        
        const char *tabs[] = {"System", "Simulation"};
        for (uint32_t i = 0; i < 2; i++) {
            bool isActive = (g_tuiState.activeEntitiesTab == i);
             CLAY(CLAY_ID_IDX("Tab", i), 
                 {.layout = {.sizing = {.height = CLAY_SIZING_FIXED(3 * TUI_CH)}, .padding = {2 * TUI_CW, 2 * TUI_CW, 1 * TUI_CH, 0}}, // Increased L/R Padding to 2
                  .backgroundColor = isActive ? (Clay_Color){40, 40, 40, 255} : (Clay_Color){20, 20, 20, 255},
                  .border = {.width = {1,1,1,1,0}, .color = isActive ? COLOR_ACCENT : (Clay_Color){60,60,60,255}}}) {
                
                Clay_Ncurses_OnClick(OnTabClick, (void*)(intptr_t)i);
                CLAY_TEXT(CLAY_STRING_DYN((char*)tabs[i]), 
                          CLAY_TEXT_CONFIG({.textColor = isActive ? COLOR_ACCENT : (Clay_Color){255, 255, 255, 255}, 
                                            .fontId = 0, .fontSize = 16})); // Normal Font
            }
        }

        // Spacer to push Filter to right (or just separate them)
        CLAY(CLAY_ID("TabFilterSpacer"), {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}}});

        // Filter Area (Inline)
        Clay_Color filterColor = g_tuiState.isFilteringEntities ? COLOR_ACCENT : (Clay_Color){120, 120, 120, 255};
        CLAY(CLAY_ID("FilterGroup"), 
             {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(30 * TUI_CW), .height = CLAY_SIZING_FIXED(1 * TUI_CH)}, // Strict Fixed Height
                         .layoutDirection = CLAY_LEFT_TO_RIGHT, 
                         .childGap = 0, // No gap, single text string
                         .padding = {0,0,0,0}}}) {
             
             bool active = g_tuiState.isFilteringEntities;
             // Logic to slice string to fit 30 chars approx
             char *rawFilter = g_tuiState.entitiesFilter;
             int len = (int)strlen(rawFilter);
             int maxLen = 15; // Conservative fit inside "[ Filter: ... ]"
             char *displayFilter = rawFilter;
             if (len > maxLen) {
                 displayFilter = rawFilter + (len - maxLen); // Show tail
             }
             
             // Format: "[ Filter: <text> ]"
             char *text = tui_ScratchFmt("[ Filter: %s%s ]", displayFilter, active ? "_" : "");
             
             CLAY_TEXT(CLAY_STRING_DYN(text), 
                       CLAY_TEXT_CONFIG({.textColor = filterColor, .fontId = 0, .fontSize = 16}));
         }
    }
    
    // Explicit Spacer for "Blank Line"
    CLAY(CLAY_ID("TopSpacer"), {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(1 * TUI_CH)}}});

    // -- Table OR Modal --
    if (g_tuiState.selectedEntityIndex != -1) {
        RenderEntityDetailModal();
    } else {
        tui_RenderDataTable(&g_ActiveTableDef, &g_tuiState.entitiesTableState, NULL);
    }
}

void tui_EntitiesHandleInput(int key) {
    // If Modal is Open, ESC closes it.
    if (g_tuiState.selectedEntityIndex != -1) {
        if (key == 27) { // ESC
            g_tuiState.selectedEntityIndex = -1;
        }
        return; // Don't process table input if modal is open
    }

    // If filtering, capture text
    if (g_tuiState.isFilteringEntities) {
        if (key == 27 || key == '\n' || key == '\r') { // ESC or Enter
            g_tuiState.isFilteringEntities = false;
            return;
        }
        if (key == KEY_BACKSPACE || key == 127 || key == 8) {
            size_t len = strlen(g_tuiState.entitiesFilter);
            if (len > 0) {
                g_tuiState.entitiesFilter[len - 1] = '\0';
                tui_UpdateEntitiesFilter();
            }
            return;
        }
        // Simple printable char check
        if (key >= 32 && key <= 126) {
            size_t len = strlen(g_tuiState.entitiesFilter);
            if (len < 63) {
                g_tuiState.entitiesFilter[len] = (char)key;
                g_tuiState.entitiesFilter[len + 1] = '\0';
                tui_UpdateEntitiesFilter();
            }
        }
        return;
    }

    DataTableState *s = &g_tuiState.entitiesTableState;
    uint32_t count = g_tuiState.filteredEntityCount; 

    if (key == KEY_DOWN) {
        if (s->selectedRowIndex < (int)count - 1) {
            s->selectedRowIndex++;
             // Keep view
            float selectionY = (float)s->selectedRowIndex * TUI_CH;
            float viewHeight = 20.0f * TUI_CH; 
            if (selectionY + s->scrollY > viewHeight) {
                s->scrollY -= TUI_CH;
            }
        }
    } else if (key == KEY_UP) {
        if (s->selectedRowIndex > 0) {
            s->selectedRowIndex--;
            if ((float)s->selectedRowIndex * TUI_CH < -s->scrollY) {
                s->scrollY += TUI_CH;
            }
        }
    } else if (key == KEY_PPAGE) { // Page Up
        s->selectedRowIndex -= 20;
        if (s->selectedRowIndex < 0) s->selectedRowIndex = 0;
        // Snap scroll
        s->scrollY = -(float)s->selectedRowIndex * TUI_CH; // Simplistic center
         if (s->scrollY > 0) s->scrollY = 0;

    } else if (key == KEY_NPAGE) { // Page Down
        s->selectedRowIndex += 20;
        if (s->selectedRowIndex >= (int)count) s->selectedRowIndex = (int)count - 1;
        
        // Snap scroll logic (keep in view)
        float selectionY = (float)s->selectedRowIndex * TUI_CH;
        float viewHeight = 20.0f * TUI_CH;
        if (selectionY + s->scrollY > viewHeight) {
             s->scrollY = viewHeight - selectionY;
        } else if (selectionY < -s->scrollY) {
             s->scrollY = -selectionY;
        }

    } else if (key == KEY_SLEFT) {
         s->scrollX += 2 * TUI_CW; 
         if (s->scrollX > 0) s->scrollX = 0;
    } else if (key == KEY_SRIGHT) {
         s->scrollX -= 2 * TUI_CW; 
    } else if (key == CTRL_KEY('f') || key == CTRL_KEY('F')) {
        g_tuiState.isFilteringEntities = !g_tuiState.isFilteringEntities;
    }
}
