#include "screen_director_ctrl.h"
#include "../tui_state.h"
#include <clay/clay.h>
#include <renderer/clay_ncurses_renderer.h>
#include <stdio.h>

void tui_InitDirectorCtrlScreen(void) {
    g_tuiState.simIsRunning = true;
    snprintf(g_tuiState.currentScenario, 64, "Morning Rush");
    g_tuiState.activeWorkers = 4;
    g_tuiState.activeUsers = 12;
}

static void OnControlClick(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
    (void)elementId;
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        int action = (int)(intptr_t)userData;
        switch (action) {
            case 0: g_tuiState.simIsRunning = !g_tuiState.simIsRunning; break;
            case 1: if (g_tuiState.activeWorkers < 50) g_tuiState.activeWorkers++; break;
            case 2: if (g_tuiState.activeWorkers > 0) g_tuiState.activeWorkers--; break;
            case 3: if (g_tuiState.activeUsers < 100) g_tuiState.activeUsers++; break;
            case 4: if (g_tuiState.activeUsers > 0) g_tuiState.activeUsers--; break;
        }
    }
}

static void RenderButton(const char *label, int action, Clay_Color bgColor) {
    CLAY(CLAY_ID_IDX("Btn", (uint32_t)action), 
        {.layout = {.padding = {2 * TUI_CW, 2 * TUI_CW, 0, 0}},
         .backgroundColor = bgColor}) {

        Clay_Ncurses_OnClick(OnControlClick, (void*)(intptr_t)action);
        bool isHovered = Clay_Hovered();

        CLAY_TEXT(CLAY_STRING_DYN((char*)label), 
            CLAY_TEXT_CONFIG({.textColor = isHovered ? (Clay_Color){255, 255, 255, 255} : (Clay_Color){200, 200, 200, 255}}));
    }
}

void tui_RenderDirectorCtrlScreen(void) {
    CLAY(CLAY_ID("DirectorWrapper"), 
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .childGap = 2 * TUI_CH,
                    .padding = {2 * TUI_CW, 2 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}}}) {

        CLAY_TEXT(CLAY_STRING("Director Manual Controls"), CLAY_TEXT_CONFIG({.textColor = COLOR_ACCENT, .fontId = CLAY_NCURSES_FONT_BOLD}));

        // Status Section
        CLAY(CLAY_ID("StatusPanel"), 
            {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()},
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap = 0.5 * TUI_CH,
                        .padding = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}},
             .border = {.width = {1, 1, 1, 1, 0}, .color = {100, 100, 100, 255}}}) {

            CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("State: %s", g_tuiState.simIsRunning ? "RUNNING" : "PAUSED")), 
                CLAY_TEXT_CONFIG({.textColor = g_tuiState.simIsRunning ? (Clay_Color){100, 255, 100, 255} : COLOR_ACCENT}));

            CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("Scenario: %s", g_tuiState.currentScenario)), CLAY_TEXT_CONFIG({.textColor = {200, 200, 200, 255}}));
            CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("Active Workers: %u", g_tuiState.activeWorkers)), CLAY_TEXT_CONFIG({.textColor = {200, 200, 200, 255}}));
            CLAY_TEXT(CLAY_STRING_DYN(tui_ScratchFmt("Active Users: %u", g_tuiState.activeUsers)), CLAY_TEXT_CONFIG({.textColor = {200, 200, 200, 255}}));
        }

        // Controls Section
        CLAY(CLAY_ID("ControlsRow"), 
            {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childGap = 2 * TUI_CW}}) {

            RenderButton(g_tuiState.simIsRunning ? "Pause" : "Resume", 0, (Clay_Color){50, 50, 150, 255});
        }

        // Spawn Controls
        CLAY(CLAY_ID("SpawnHeader"), {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()}}}) {
            CLAY_TEXT(CLAY_STRING("Management & Spawning"), CLAY_TEXT_CONFIG({.textColor = {150, 150, 150, 255}, .fontId = CLAY_NCURSES_FONT_BOLD}));
        }

        CLAY(CLAY_ID("SpawnRows"), 
            {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()},
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap = 1 * TUI_CH}}) {

            CLAY(CLAY_ID("WorkerRow"), {.layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = 2 * TUI_CW, .sizing = {.width = CLAY_SIZING_GROW()}}}) {
                CLAY(CLAY_ID("WLabel"), {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(10 * TUI_CW)}}}) {
                    CLAY_TEXT(CLAY_STRING("Workers:"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}}));
                }
                RenderButton("+ Add", 1, (Clay_Color){0, 100, 0, 255});
                RenderButton("- Rem", 2, (Clay_Color){100, 0, 0, 255});
            }

            CLAY(CLAY_ID("UserRow"), {.layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = 2 * TUI_CW, .sizing = {.width = CLAY_SIZING_GROW()}}}) {
                CLAY(CLAY_ID("ULabel"), {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(10 * TUI_CW)}}}) {
                    CLAY_TEXT(CLAY_STRING("Users:  "), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}}));
                }
                RenderButton("+ Add", 3, (Clay_Color){0, 100, 0, 255});
                RenderButton("- Rem", 4, (Clay_Color){100, 0, 0, 255});
            }
        }
    }
}
