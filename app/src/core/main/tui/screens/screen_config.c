#include "screen_config.h"
#include "../tui_state.h"
#include "utils/configs.h"
#include <renderer/clay_ncurses_renderer.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>

// Helper to flatten config for display
static void flatten_cb(const char *section, const char *key, const char *value, void *user_data) {
    (void)user_data;
    if (g_tuiState.configDisplayCount >= 128) return;
    
    ConfigItem *item = &g_tuiState.configDisplayItems[g_tuiState.configDisplayCount++];
    strncpy(item->section, section, 63); item->section[63] = '\0';
    strncpy(item->key, key, 63); item->key[63] = '\0';
    strncpy(item->value, value, 255); item->value[255] = '\0';
    
    if (section[0]) snprintf(item->displayKey, 128, "%s.%s", section, key);
    else snprintf(item->displayKey, 128, "%s", key);
}

static void RefreshConfigDisplay(void) {
    g_tuiState.configDisplayCount = 0;
    g_tuiState.maxKeyLength = 0;
    if (g_tuiState.loadedConfigHandle) {
        po_config_foreach(g_tuiState.loadedConfigHandle, flatten_cb, NULL);
    }

    // Calculate max key length (in visual chars)
    for (uint32_t i = 0; i < g_tuiState.configDisplayCount; i++) {
        uint32_t len = (uint32_t)strlen(g_tuiState.configDisplayItems[i].displayKey);
        if (len > g_tuiState.maxKeyLength) g_tuiState.maxKeyLength = len;
    }
    // Min width buffer
    if (g_tuiState.maxKeyLength < 10) g_tuiState.maxKeyLength = 10;
}

static void LoadConfigTab(uint32_t index) {
    if (index >= g_tuiState.configFileCount) return;
    
    // Free previous
    if (g_tuiState.loadedConfigHandle) {
         po_config_free((po_config_t**)&g_tuiState.loadedConfigHandle);
    }
    
    char path[128];
    snprintf(path, sizeof(path), "config/%s", g_tuiState.configFiles[index]);
    
    if (po_config_load(path, (po_config_t**)&g_tuiState.loadedConfigHandle) == 0) {
         g_tuiState.activeConfigTab = index;
         RefreshConfigDisplay();
         g_tuiState.selectedConfigItemIndex = 0;
         g_tuiState.lastSelectedConfigItemIndex = 0;
         g_tuiState.configScrollY = 0;
         g_tuiState.isEditing = false;
         g_tuiState.lastSavedMessage[0] = '\0';
    } else {
        // Handle error?
        snprintf(g_tuiState.lastSavedMessage, 64, "Error loading %s", g_tuiState.configFiles[index]);
    }
}

void tui_InitConfigScreen(void) {
    g_tuiState.configFileCount = 0;
    g_tuiState.activeConfigTab = 0;
    g_tuiState.configScrollY = 0;
    g_tuiState.configFileCount = 0;
    g_tuiState.activeConfigTab = 0;
    g_tuiState.configScrollY = 0;
    g_tuiState.loadedConfigHandle = NULL;
    g_tuiState.isEditing = false;
    g_tuiState.lastSavedMessage[0] = '\0';
    g_tuiState.hoveredConfigItemIndex = UINT32_MAX;

    DIR *d;
    struct dirent *dir;
    d = opendir("config"); 
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            size_t len = strlen(dir->d_name);
            if (len > 4 && strcmp(dir->d_name + len - 4, ".ini") == 0) {
                if (g_tuiState.configFileCount < 16) {
                    strncpy(g_tuiState.configFiles[g_tuiState.configFileCount], dir->d_name, 63);
                    g_tuiState.configFileCount++;
                }
            }
        }
        closedir(d);
        
        // Sort files alphabetically? (Simple bubble sort for UI consistency)
        for (uint32_t i = 0; i < g_tuiState.configFileCount; i++) {
            for (uint32_t j = i + 1; j < g_tuiState.configFileCount; j++) {
                if (strcmp(g_tuiState.configFiles[i], g_tuiState.configFiles[j]) > 0) {
                    char temp[64];
                    strcpy(temp, g_tuiState.configFiles[i]);
                    strcpy(g_tuiState.configFiles[i], g_tuiState.configFiles[j]);
                    strcpy(g_tuiState.configFiles[j], temp);
                }
            }
        }
    }
    
    // Load the first one if available
    if (g_tuiState.configFileCount > 0) {
        LoadConfigTab(0);
    }
}

static void OnTabClick(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
    (void)elementId;
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        int index = (int)(intptr_t)userData;
        if (!g_tuiState.isEditing) { // Prevent tab switch while editing
            LoadConfigTab((uint32_t)index);
        }
    }
}

static void OnItemInteract(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
    (void)elementId;
    int index = (int)(intptr_t)userData;

    // Hover Logic
    g_tuiState.hoveredConfigItemIndex = (uint32_t)index;

    // Click Logic
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        if (!g_tuiState.isEditing) {
            g_tuiState.selectedConfigItemIndex = (uint32_t)index;
        }
    }
}
void tui_ConfigNextTab(void) {
    if (g_tuiState.configFileCount > 0) {
        uint32_t next = (g_tuiState.activeConfigTab + 1) % g_tuiState.configFileCount;
        LoadConfigTab(next);
    }
}

void tui_ConfigPrevTab(void) {
    if (g_tuiState.configFileCount > 0) {
        uint32_t prev = (g_tuiState.activeConfigTab - 1 + g_tuiState.configFileCount) % g_tuiState.configFileCount;
        LoadConfigTab(prev);
    }
}

void tui_ConfigEnterEdit(void) {
    if (g_tuiState.configDisplayCount > 0) {
        g_tuiState.isEditing = true;
        ConfigItem *item = &g_tuiState.configDisplayItems[g_tuiState.selectedConfigItemIndex];
        strncpy(g_tuiState.editValueBuffer, item->value, 255);
        g_tuiState.editValueBuffer[255] = '\0';
    }
}

void tui_ConfigCommitEdit(void) {
    if (g_tuiState.isEditing && g_tuiState.loadedConfigHandle) {
        ConfigItem *item = &g_tuiState.configDisplayItems[g_tuiState.selectedConfigItemIndex];

        // Update in memory config
        if (po_config_set_str(g_tuiState.loadedConfigHandle, item->section, item->key, g_tuiState.editValueBuffer) == 0) {
            // Update display cache
            strncpy(item->value, g_tuiState.editValueBuffer, 255);
            g_tuiState.isEditing = false;
        }
    }
}

void tui_ConfigCancelEdit(void) {
    g_tuiState.isEditing = false;
}

void tui_ConfigAppendChar(char c) {
    size_t len = strlen(g_tuiState.editValueBuffer);
    if (len < 255) {
        g_tuiState.editValueBuffer[len] = c;
        g_tuiState.editValueBuffer[len+1] = '\0';
    }
}

void tui_ConfigBackspace(void) {
    size_t len = strlen(g_tuiState.editValueBuffer);
    if (len > 0) {
        g_tuiState.editValueBuffer[len-1] = '\0';
    }
}

void tui_SaveCurrentConfig(void) {
    if (g_tuiState.loadedConfigHandle && g_tuiState.configFileCount > 0) {
        char path[128];
        snprintf(path, sizeof(path), "config/%s", g_tuiState.configFiles[g_tuiState.activeConfigTab]);

        if (po_config_save(g_tuiState.loadedConfigHandle, path) == 0) {
            snprintf(g_tuiState.lastSavedMessage, 64, "Saved to %s", g_tuiState.configFiles[g_tuiState.activeConfigTab]);
        } else {
            snprintf(g_tuiState.lastSavedMessage, 64, "Error Saving!");
        }
    }
}

void tui_RenderConfigScreen(void) {
    CLAY(CLAY_ID("ConfigScreen"), 
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 0}}) {

        // 1. Tabs Header
        CLAY(CLAY_ID("ConfigTabs"),
            {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(3 * TUI_CH)},
                        .padding = {1 * TUI_CW, 1 * TUI_CW, 0, 0},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childGap = 1 * TUI_CW},
             .backgroundColor = {20, 20, 20, 255},
             .border = {.width = {0, 0, 0, 1, 0}, .color = {50, 50, 50, 255}}}) {

             for (uint32_t i = 0; i < g_tuiState.configFileCount; i++) {
                 bool isActive = (i == g_tuiState.activeConfigTab);
                 CLAY(CLAY_ID_IDX("ConfigTab", i),
                      {.layout = {.sizing = {.width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW()},
                                  .padding = {1 * TUI_CW, 1 * TUI_CW, 0, 0}},
                       .backgroundColor = isActive ? (Clay_Color){80, 100, 160, 255} : (Clay_Color){30, 30, 30, 255}}) {

                       Clay_Ncurses_OnClick(OnTabClick, (void*)(intptr_t)i);
                       CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("%s", g_tuiState.configFiles[i])), 
                                 CLAY_TEXT_CONFIG({.textColor = isActive ? (Clay_Color){255, 255, 255, 255} : COLOR_TEXT_DIM, 
                                                   .fontId = isActive ? CLAY_NCURSES_FONT_BOLD : 0}));
                 }
             }
        }

        // 2. Toolbar / Status
        CLAY(CLAY_ID("ConfigTools"),
             {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(2 * TUI_CH)},
                         .padding = {1 * TUI_CW, 1 * TUI_CW, 0, 0},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 2 * TUI_CW},
              .backgroundColor = {25, 25, 25, 255}}) {

              if (g_tuiState.isEditing) {
                   CLAY_TEXT(CLAY_STRING("EDIT MODE - [ENTER] Confirm, [ESC] Cancel"), CLAY_TEXT_CONFIG({.textColor = {255, 200, 100, 255}}));
              } else {
                   CLAY_TEXT(CLAY_STRING("[ENTER] Edit   [Ctrl+S] Save"), CLAY_TEXT_CONFIG({.textColor = {150, 150, 150, 255}}));
              }

              if (g_tuiState.lastSavedMessage[0]) {
                   CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("%s", g_tuiState.lastSavedMessage)), CLAY_TEXT_CONFIG({.textColor = {100, 255, 100, 255}}));
              }
        }

        // 3. Main Editor Area
        CLAY(CLAY_ID("ConfigEditor"),
             {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .padding = {0, 0, 1 * TUI_CH, 0}},
              .clip = {.vertical = true, .childOffset = {0, -g_tuiState.configScrollY}}}) {

              for (uint32_t i = 0; i < g_tuiState.configDisplayCount; i++) {
                  bool isSelected = (i == g_tuiState.selectedConfigItemIndex);
                  bool isHovered = (i == g_tuiState.hoveredConfigItemIndex);
                  ConfigItem *item = &g_tuiState.configDisplayItems[i];

                  Clay_Color rowColor = {0,0,0,0};
                  if (isSelected) rowColor = (Clay_Color){0, 95, 255, 255}; // Bright Blue
                  else if (isHovered) rowColor = (Clay_Color){60, 60, 60, 255}; // Grey

                  CLAY(CLAY_ID_IDX("ConfigItemRow", i),
                       {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(2 * TUI_CH)},
                                   .padding = {1 * TUI_CW, 1 * TUI_CW, 0, 0},
                                   .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                   .childGap = 1 * TUI_CW},
                        .backgroundColor = rowColor}) {

                        Clay_OnHover(OnItemInteract, (void*)(intptr_t)i);

                        // Section.Key
                        uint32_t keyColWidth = g_tuiState.maxKeyLength * TUI_CW + 2 * TUI_CW; // Chars * Width + padding
                        CLAY(CLAY_ID_IDX("ConfigKeyPart", i), 
                             {.layout = {.sizing = {.width = CLAY_SIZING_FIXED((float)keyColWidth), .height = CLAY_SIZING_GROW()}}}) {
                            CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("%s", item->displayKey)), CLAY_TEXT_CONFIG({.textColor = isSelected ? (Clay_Color){255,255,255,255} : COLOR_ACCENT}));
                        }

                        // Value
                        CLAY(CLAY_ID_IDX("ConfigValPart", i),
                              {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}}}) {

                            if (isSelected && g_tuiState.isEditing) {
                                // Render Input Field
                                CLAY(CLAY_ID("EditField"),
                                      {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}},
                                       .backgroundColor = {0, 0, 0, 255}}) {

                                     // Render actual value with cursor
                                     char *display = tui_ScratchFmt("> %s_", g_tuiState.editValueBuffer);
                                     CLAY_TEXT(CLAY_STRING_DYN(display), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}}));
                                }
                            } else {
                                CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("%s", item->value)), CLAY_TEXT_CONFIG({.textColor = {200, 200, 200, 255}}));
                            }
                        }
                  }
              }
        }
    }
}
