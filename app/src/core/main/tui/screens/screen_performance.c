#include "screen_performance.h"
#include "../tui_state.h"
#include <clay/clay.h>
#include <renderer/clay_ncurses_renderer.h>

/**
 * @brief Internal helper to render a row of tabs.
 * 
 * @param titles Array of tab title strings.
 * @param count Number of tabs to render.
 * @param activeIndex Currently active tab index.
 */
static void RenderTabs(const char *titles[], uint32_t count, uint32_t activeIndex) {
    CLAY_AUTO_ID(
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()},
                     .childGap = 2 * TUI_CW,
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .padding = {1 * TUI_CW, 0, 0, 0}}}) {
        for (uint32_t i = 0; i < count; i++) {
            bool isActive = (i == activeIndex);

            // Generate a unique ID for each tab based on its index
            CLAY(CLAY_ID_IDX("Tab", i), 
                {.layout = {.padding = {2 * TUI_CW, 2 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}},
                 .border = {.width = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH, 0}, 
                            .color = isActive ? COLOR_ACCENT : COLOR_TEXT_DIM}}) {

                CLAY_TEXT(CLAY_STRING_DYN((char*)titles[i]), 
                    CLAY_TEXT_CONFIG({.textColor = isActive ? COLOR_ACCENT : COLOR_TEXT_DIM,
                                      .fontId = isActive ? CLAY_NCURSES_FONT_BOLD : 0}));
            }
        }
    }
}

/**
 * @brief Renders the Performance Monitor content.
 * 
 * Structure:
 * - Tabs: System, Libraries, Stats.
 * - Content: Metrics based on active tab.
 */
void tui_RenderPerformanceScreen(void) {
    const char *tabs[] = {"System", "Libraries", "Stats"};
    RenderTabs(tabs, 3, g_tuiState.activePerfTab);

    CLAY(CLAY_ID("PerfContent"), 
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .padding = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}},
         .border = {.width = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH, 0}, 
                    .color = {255, 255, 255, 255}}}) {

        CLAY_TEXT(CLAY_STRING("Performance Metrics - Placeholder"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}}));
    }
}
