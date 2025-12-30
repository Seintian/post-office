#include "app_tui.h"

#ifndef CLAY_IMPLEMENTATION
#include <clay/clay.h>
#endif

#include <renderer/clay_ncurses_renderer.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

// --- Dashboard State ---
// This would ideally come from the simulation, for now we mock it or just show static UI.

// --- Helper Macros ---

#define DASHBOARD_PANEL(id)                                                                        \
    CLAY(CLAY_ID(id),                                                                              \
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},        \
                     .padding = {1, 1, 1, 1},                                                      \
                     .childGap = 1,                                                                \
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},                                       \
          .border = {.width = CLAY_BORDER_OUTSIDE(1), .color = {255, 255, 255, 255}}})

static void DashboardItem(Clay_String label, Clay_String value) {
    CLAY(CLAY_ID("DashboardItem"),
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(1)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 2}}) {
        CLAY_TEXT(label, CLAY_TEXT_CONFIG({.textColor = {200, 200, 200, 255}}));
        CLAY_TEXT(value, CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255},
                                           .fontId = CLAY_NCURSES_FONT_BOLD}));
    }
}

// --- Main Loop ---

static void RunDashboardLoop(void) {
    // 1. Initialize
    // Arena size: 16MB
    uint64_t totalMemorySize = 16 * 1024 * 1024;
    Clay_Arena arena =
        Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
    Clay_Initialize(arena, (Clay_Dimensions){0, 0}, (Clay_ErrorHandler){0});
    Clay_SetMeasureTextFunction(Clay_Ncurses_MeasureText, NULL);

    Clay_Ncurses_Initialize();

    bool running = true;
    while (running) {
        // 2. Input
        int key = Clay_Ncurses_ProcessInput(stdscr);
        if (key == 'q' || key == 27) {
            running = false;
        }

        // 3. Layout Update
        Clay_SetLayoutDimensions(Clay_Ncurses_GetLayoutDimensions());

        Clay_BeginLayout();

        CLAY(CLAY_ID("Root"),
             {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .backgroundColor = {0, 0, 0, 255}}) {
            // Header
            CLAY(
                CLAY_ID("Header"),
                {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(3)},
                            .padding = {2, 1, 0, 0},
                            .childGap = 2},
                 .border = {.width = {.bottom = 1}, .color = {255, 255, 255, 255}}}) {
                CLAY_TEXT(CLAY_STRING("POST OFFICE SIMULATION"),
                          CLAY_TEXT_CONFIG({.fontId = CLAY_NCURSES_FONT_BOLD}));
                CLAY_TEXT(CLAY_STRING("|  Real-time Dashboard"),
                          CLAY_TEXT_CONFIG({.textColor = {150, 150, 150, 255}}));
            }

            // Main Content Area
            CLAY(CLAY_ID("Body"),
                 {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .padding = {1, 1, 1, 1},
                             .childGap = 1}}) {
                // Left Column: Stats
                DASHBOARD_PANEL("StatsPanel") {
                    CLAY_TEXT(CLAY_STRING("[ System Status ]"),
                              CLAY_TEXT_CONFIG({.fontId = CLAY_NCURSES_FONT_UNDERLINE}));
                    DashboardItem(CLAY_STRING("Status:"), CLAY_STRING("Running"));
                    DashboardItem(CLAY_STRING("Uptime:"), CLAY_STRING("00:00:10")); // Mock
                    DashboardItem(CLAY_STRING("Memory:"), CLAY_STRING("128 MB"));

                    CLAY_AUTO_ID(
                        {.layout = {.sizing = {.height = CLAY_SIZING_FIXED(1)}}}); // Spacer

                    CLAY_TEXT(CLAY_STRING("[ Queue Metrics ]"),
                              CLAY_TEXT_CONFIG({.fontId = CLAY_NCURSES_FONT_UNDERLINE}));
                    DashboardItem(CLAY_STRING("Pending Users:"), CLAY_STRING("42"));
                    DashboardItem(CLAY_STRING("Active Workers:"), CLAY_STRING("5"));
                }

                // Right Column: Logs or Activity
                DASHBOARD_PANEL("ActivityPanel") {
                    CLAY_TEXT(CLAY_STRING("[ Recent Activity ]"),
                              CLAY_TEXT_CONFIG({.fontId = CLAY_NCURSES_FONT_UNDERLINE}));
                    CLAY_TEXT(CLAY_STRING("> User 101 entered queue"),
                              CLAY_TEXT_CONFIG({.textColor = {100, 255, 100, 255}}));
                    CLAY_TEXT(CLAY_STRING("> Worker A finished task"),
                              CLAY_TEXT_CONFIG({.textColor = {100, 200, 255, 255}}));
                    CLAY_TEXT(CLAY_STRING("> Issue detected in Logic"),
                              CLAY_TEXT_CONFIG({.textColor = {255, 100, 100, 255}}));
                }
            }

            // Footer
            CLAY(CLAY_ID("Footer"), {.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                                           .height = CLAY_SIZING_FIXED(1)},
                                                .padding = {2, 0, 0, 0}}}) {
                CLAY_TEXT(CLAY_STRING("Press 'q' to exit simulation."),
                          CLAY_TEXT_CONFIG({.textColor = {120, 120, 120, 255}}));
            }
        }

        Clay_RenderCommandArray commands = Clay_EndLayout();
        Clay_Ncurses_Render(commands);

        // Sleep to cap framerate ~30fps
        usleep(33000);
    }

    Clay_Ncurses_Terminate();
}

void app_tui_run_demo(void) {
    RunDashboardLoop();
}

void app_tui_run_simulation(void) {
    RunDashboardLoop();
}
