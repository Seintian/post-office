#include "screen_logs.h"
#include "../tui_state.h"
#include <clay/clay.h>
#include <renderer/clay_ncurses_renderer.h>
#include "../components/log_tail_view.h"

#include <dirent.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/tui_registry.h"

static void tui_RefreshLogs(void) {
    g_tuiState.logFileCount = 0;

    DIR *d;
    struct dirent *dir;
    d = opendir("logs"); 
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            size_t len = strlen(dir->d_name);
            if (len > 4 && strcmp(dir->d_name + len - 4, ".log") == 0) {
                if (g_tuiState.logFileCount < 16) {
                    strncpy(g_tuiState.logFiles[g_tuiState.logFileCount], dir->d_name, 63);
                    g_tuiState.logFiles[g_tuiState.logFileCount][63] = '\0';
                    g_tuiState.logFileCount++;
                }
            }
        }
        closedir(d);
    }

    // Simple sort
    if (g_tuiState.logFileCount > 0) {
        for (uint32_t i = 0; i < g_tuiState.logFileCount - 1; i++) {
            for (uint32_t j = 0; j < g_tuiState.logFileCount - i - 1; j++) {
                if (strcmp(g_tuiState.logFiles[j], g_tuiState.logFiles[j + 1]) > 0) {
                    char temp[64];
                    strcpy(temp, g_tuiState.logFiles[j]);
                    strcpy(g_tuiState.logFiles[j], g_tuiState.logFiles[j + 1]);
                    strcpy(g_tuiState.logFiles[j + 1], temp);
                }
            }
        }
    }
}

static void cmd_logs_refresh(tui_context_t* ctx, void* user_data) {
    (void)ctx; (void)user_data;
    tui_RefreshLogs();
}

void tui_InitLogsScreen(void) {
    g_tuiState.logFileCount = 0;
    g_tuiState.activeLogTab = 0;

    tui_RefreshLogs();

    // Register Refresh Command
    tui_registry_register_command("logs.refresh", "Refresh Log Files", cmd_logs_refresh, NULL);
    tui_registry_register_binding(CTRL_KEY('r'), false, TUI_BINDING_context_logs, "logs.refresh");
}

void OnLogTabClick(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
    (void)elementId;
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        int index = (int)(intptr_t)userData;
        if (index >= 0 && index < (int)g_tuiState.logFileCount) {
            g_tuiState.activeLogTab = (uint32_t)index;
            g_tuiState.logScrollPosition = (Clay_Vector2){0,0};
            g_tuiState.logReadOffset = -1;
        }
    }
}

void tui_RenderLogsScreen(void) {
    // Render Tabs
    CLAY(CLAY_ID("LogsTabs"),
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()},
                     .childGap = 2 * TUI_CW,
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .padding = {0, 0, 0, 0}}}) {

        for (uint32_t i = 0; i < g_tuiState.logFileCount; i++) {

            bool isActive = (i == g_tuiState.activeLogTab);

            CLAY(CLAY_ID_IDX("LogTab", i), 
                {.layout = {.padding = {2 * TUI_CW, 2 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}},
                 .border = {.width = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH, 0}, 
                            .color = isActive ? COLOR_ACCENT : (Clay_Color){100, 100, 100, 255}}}) {

                Clay_Ncurses_OnClick(OnLogTabClick, (void*)(intptr_t)i);
                bool isHovered = Clay_Hovered();

                CLAY_TEXT(CLAY_STRING_DYN(g_tuiState.logFiles[i]), 
                    CLAY_TEXT_CONFIG({.textColor = (isActive || isHovered) ? COLOR_ACCENT : (Clay_Color){120, 120, 120, 255},
                                      .fontId = isActive ? CLAY_NCURSES_FONT_BOLD : 0}));
            }
        }
    }

    // Render Content Area
    CLAY(CLAY_ID("LogsContent"), 
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .padding = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}},
         .border = {.width = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH, 0}, 
                    .color = {255, 255, 255, 255}}}) {

        if (g_tuiState.logFileCount > 0) {
            tui_RenderLogTailView(g_tuiState.logFiles[g_tuiState.activeLogTab]);
        } else {
            CLAY_TEXT(CLAY_STRING("No logs found in logs/ directory."), CLAY_TEXT_CONFIG({.textColor = COLOR_ERROR}));
        }
    }
}

