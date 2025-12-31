#include "bottombar.h"
#include "../tui_state.h"
#include <clay/clay.h>
#include <renderer/clay_ncurses_renderer.h>
#include <stdint.h>

/**
 * @brief Renders the bottom footer bar.
 * 
 * Structure:
 * - Left: Prompt ">" and Input Buffer.
 * - Input Buffer: Handles scrolling if text exceeds view width and renders the cursor.
 * - Right: Keybinding guide.
 */
void tui_RenderBottomBar(void) {
    CLAY(CLAY_ID("Footer"),
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(3 * TUI_CH)},
                     .padding = {2 * TUI_CW, 2 * TUI_CW, 0, 0},
                     .childGap = 0,
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .border = {.width = {0, 0, 1 * TUI_CH, 0, 0}, .color = {255, 255, 255, 255}}}) {

        // Command Prompt
        CLAY_TEXT(CLAY_STRING("> "), CLAY_TEXT_CONFIG({.fontId = CLAY_NCURSES_FONT_BOLD, .textColor = {0, 255, 0, 255}}));

        // Input Field Container
        CLAY(CLAY_ID("InputContainer"), 
            {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(50 * TUI_CW), .height = CLAY_SIZING_FIXED(1 * TUI_CH)},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childAlignment = {.y = CLAY_ALIGN_Y_TOP}}}) {

            // Simple horizontal scrolling for input
            uint32_t viewWidth = 50;
            uint32_t start = (g_tuiState.inputCursor > viewWidth) ? g_tuiState.inputCursor - viewWidth : 0;
            Clay_String inputStr = CLAY_STRING_DYN(&g_tuiState.inputBuffer[start]);

            CLAY_TEXT(inputStr, CLAY_TEXT_CONFIG({.textColor = {0, 255, 0, 255}, .fontId = CLAY_NCURSES_FONT_BOLD}));

            // Cursor Block
            CLAY(CLAY_ID("Cursor"), 
                {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(1 * TUI_CW), .height = CLAY_SIZING_FIXED(1 * TUI_CH)}},
                 .backgroundColor = {0, 255, 0, 255}});
        }

        // Spacer
        CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW()}}}); 

        // Help Hints
        CLAY_TEXT(CLAY_STRING("[F1] Sim  [F2] Perf  [TAB] Switch Tab  [Ctrl+q] Quit"), 
                  CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT_DIM}));
    }
}
