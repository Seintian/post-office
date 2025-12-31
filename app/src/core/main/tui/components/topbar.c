#include "topbar.h"
#include "../tui_state.h"
#include <clay/clay.h>
#include <renderer/clay_ncurses_renderer.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Renders the top navigation bar.
 * 
 * Structure:
 * - Left: "POST OFFICE" Title.
 * - Center-Left: Current Screen Name (dynamic).
 * - Right: System Stats (FPS, CPU, MEM).
 */
void tui_RenderTopBar(void) {
    CLAY(CLAY_ID("Header"),
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(2 * TUI_CH)},
                     .padding = {2 * TUI_CW, 2 * TUI_CW, 0, 0},
                     .childGap = 4 * TUI_CW,
                     .layoutDirection = CLAY_LEFT_TO_RIGHT, // Horizontal layout
                     .childAlignment = {.y = CLAY_ALIGN_Y_TOP}},
          .border = {.width = {0, 0, 0, 1 * TUI_CH, 0}, .color = {255, 255, 255, 255}}}) {

        // App Title
        CLAY_TEXT(CLAY_STRING("POST OFFICE"),
                  CLAY_TEXT_CONFIG({.fontId = CLAY_NCURSES_FONT_BOLD, .textColor = {255, 255, 255, 255}}));

        // Current Screen Indicator
        const char *screenName = (g_tuiState.currentScreen == SCREEN_SIMULATION) ? "SIMULATION" : "PERFORMANCE";
        CLAY(CLAY_ID("ScreenTitle"), {.layout = {.padding = {1 * TUI_CW, 1 * TUI_CW, 0, 0}}}) {
             CLAY_TEXT(CLAY_STRING_DYN((char*)screenName), CLAY_TEXT_CONFIG({.textColor = {150, 255, 150, 255}}));
        }

        // Spacer to push stats to the right
        CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW()}}}); 

        // System Stats
        static char statsBuffer[64];
        snprintf(statsBuffer, sizeof(statsBuffer), "FPS: %.0f | CPU: %.1f%% | MEM: %.0fMB", 
                 (double)g_tuiState.fps, (double)g_tuiState.cpuUsage, (double)g_tuiState.memUsage);
        CLAY_TEXT(CLAY_STRING_DYN(statsBuffer), CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT_DIM}));
    }
}
