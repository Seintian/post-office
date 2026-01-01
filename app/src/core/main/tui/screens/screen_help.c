#include "screen_help.h"
#include "../tui_state.h"
#include "../components/data_table.h"
#include <clay/clay.h>
#include <renderer/clay_ncurses_renderer.h>
#include <stdio.h>

extern const DataTableAdapter g_HelpAdapter;

static const DataTableDef g_HelpTableDef = {
    .columns = {
        {0, "Key", 20.0f, true},
        {1, "Description", 40.0f, true},
        {2, "Context", 15.0f, true}
    },
    .columnCount = 3,
};

static DataTableDef g_ActiveHelpTableDef;
static bool g_HelpInitialized = false;

static void InitHelpTableDef(void) {
    g_ActiveHelpTableDef = g_HelpTableDef;
    g_ActiveHelpTableDef.adapter = g_HelpAdapter;
    g_HelpInitialized = true;
}

void tui_InitHelpScreen(void) {
    g_tuiState.helpBindingCount = 0;
    Keybinding *kb = g_tuiState.helpBindings;
    uint32_t i = 0;

    // --- Global Navigation ---
    snprintf(kb[i].key, 32, "F1 - F8");
    snprintf(kb[i].description, 128, "Directly switch to screen by index");
    snprintf(kb[i++].context, 32, "Global");

    snprintf(kb[i].key, 32, "Ctrl + /");
    snprintf(kb[i].description, 128, "Focus filter field");
    snprintf(kb[i++].context, 32, "Entities");

    // --- Simulation & Director ---
    snprintf(kb[i].key, 32, "Ctrl+W / Ctrl+E");
    snprintf(kb[i].description, 128, "Increase / Decrease worker threads");
    snprintf(kb[i++].context, 32, "Director Ctrl");

    snprintf(kb[i].key, 32, "Ctrl+U / Ctrl+Y");
    snprintf(kb[i].description, 128, "Increase / Decrease concurrent users");
    snprintf(kb[i++].context, 32, "Director Ctrl");

    // --- Configuration Editor ---
    snprintf(kb[i].key, 32, "Enter");
    snprintf(kb[i].description, 128, "Enter edit mode for selected field");
    snprintf(kb[i++].context, 32, "Config");

    snprintf(kb[i].key, 32, "Esc");
    snprintf(kb[i].description, 128, "Cancel editing / Back");
    snprintf(kb[i++].context, 32, "Config");

    snprintf(kb[i].key, 32, "Ctrl + S");
    snprintf(kb[i].description, 128, "Save configuration changes to disk");
    snprintf(kb[i++].context, 32, "Config");


    g_tuiState.helpBindingCount = i;
    g_tuiState.helpTableState = (DataTableState){0, 0, 0, 0, true, -1, -1};
}

void tui_RenderHelpScreen(void) {
    if (!g_HelpInitialized) InitHelpTableDef();

    // Lazy Init fallback
    if (g_tuiState.helpBindingCount == 0) {
        tui_InitHelpScreen();
    }

    CLAY(CLAY_ID("HelpWrapper"), 
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .childGap = 1 * TUI_CH,
                    .padding = {2 * TUI_CW, 2 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}}}) {

        CLAY_TEXT(CLAY_STRING("Help & Shortcuts"), CLAY_TEXT_CONFIG({.textColor = COLOR_ACCENT, .fontId = CLAY_NCURSES_FONT_BOLD}));

        // Render the generic DataTable
        tui_RenderDataTable(&g_ActiveHelpTableDef, &g_tuiState.helpTableState, NULL);

        // Legend Section
        CLAY(CLAY_ID("LegendTitle"), {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()}}}) {
            CLAY_TEXT(CLAY_STRING("Visual Legend"), CLAY_TEXT_CONFIG({.textColor = {200, 200, 200, 255}, .fontId = CLAY_NCURSES_FONT_BOLD}));
        }

        CLAY(CLAY_ID("LegendContent"), 
            {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childGap = 4 * TUI_CW,
                        .padding = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}}}) {

            // Accent
            CLAY(CLAY_ID("LegendAccent"), {.layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = 1 * TUI_CW}}) {
                CLAY(CLAY_ID("AccentBox"), {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(2 * TUI_CW), .height = CLAY_SIZING_FIXED(1 * TUI_CH)}}, .backgroundColor = COLOR_ACCENT});
                CLAY_TEXT(CLAY_STRING("Selection / Active"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}}));
            }

            // Error
            CLAY(CLAY_ID("LegendError"), {.layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = 1 * TUI_CW}}) {
                CLAY(CLAY_ID("ErrorBox"), {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(2 * TUI_CW), .height = CLAY_SIZING_FIXED(1 * TUI_CH)}}, .backgroundColor = COLOR_ERROR});
                CLAY_TEXT(CLAY_STRING("Error / Warning"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}}));
            }

            // Dim
            CLAY(CLAY_ID("LegendDim"), {.layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = 1 * TUI_CW}}) {
                CLAY(CLAY_ID("DimBox"), {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(2 * TUI_CW), .height = CLAY_SIZING_FIXED(1 * TUI_CH)}}, .backgroundColor = COLOR_TEXT_DIM});
                CLAY_TEXT(CLAY_STRING("Inactive / Secondary"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}}));
            }
        }
    }
}

bool tui_HelpHandleInput(int key) {
    return tui_DataTableHandleInput(&g_tuiState.helpTableState, g_tuiState.helpBindingCount, key);
}
