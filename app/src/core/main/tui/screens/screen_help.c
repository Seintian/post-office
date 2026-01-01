#include "screen_help.h"
#include "../tui_state.h"
#include "../core/tui_registry.h"
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

static const char* _context_to_str(tui_binding_context_t ctx) {
    switch (ctx) {
        case TUI_BINDING_context_global: return "Global";
        case TUI_BINDING_context_simulation: return "Simulation";
        case TUI_BINDING_context_config: return "Config";
        case TUI_BINDING_context_logs: return "Logs";
        case TUI_BINDING_context_editor: return "Editor";
        case TUI_BINDING_context_entities: return "Entities";
        case TUI_BINDING_CONTEXT_COUNT:
        default: return "Unknown";
    }
}

static void _format_key(int key, bool ctrl, char* buf, size_t size) {
    // If explicitly marked as CTRL modifier in registry
    if (ctrl) {
        if (key >= 'a' && key <= 'z') key -= 32; // Toupper
        snprintf(buf, size, "Ctrl + %c", key);
        return;
    }

    // Handle Control Codes (1-26) embedded in keycode
    if (key >= 1 && key <= 26) {
        // Exceptions for standard keys that share codes
        if (key == 10) { snprintf(buf, size, "Enter"); return; } // Ctrl+J / Enter
        if (key == 9)  { snprintf(buf, size, "Tab"); return; }   // Ctrl+I / Tab
        if (key == 27) { snprintf(buf, size, "Esc"); return; }   // Esc (27)

        // Otherwise, render as Ctrl + Letter
        snprintf(buf, size, "Ctrl + %c", 'A' + key - 1);
        return;
    }

    // Function Keys
    if (key >= KEY_F(1) && key <= KEY_F(12)) {
        snprintf(buf, size, "F%d", key - KEY_F(0));
        return;
    }

    // Special Keys
    if (key == 27) { snprintf(buf, size, "Esc"); return; }
    if (key == 127 || key == KEY_BACKSPACE) { snprintf(buf, size, "Backspace"); return; }

    // Printable
    if (key >= 32 && key <= 126) {
        snprintf(buf, size, "%c", key);
    } else {
        snprintf(buf, size, "[%d]", key); // Fallback for debug
    }
}

void tui_InitHelpScreen(void) {
    g_tuiState.helpBindingCount = 0;

    // Fetch from Registry
    uint32_t count = 0;
    const tui_keybinding_t* bindings = tui_registry_get_bindings(&count);

    for (uint32_t j = 0; j < count; j++) {
        if (g_tuiState.helpBindingCount >= 128) break; // Hard limit in tuiState

        const tui_keybinding_t* b = &bindings[j];
        Keybinding *kb = &g_tuiState.helpBindings[g_tuiState.helpBindingCount];

        // Key
        _format_key(b->key_code, b->ctrl_modifier, kb->key, sizeof(kb->key));

        // Description
        const char* desc = tui_registry_get_command_desc(b->command_id);
        if (desc) {
            snprintf(kb->description, sizeof(kb->description), "%s", desc);
        } else {
            snprintf(kb->description, sizeof(kb->description), "%s", b->command_id);
        }

        // Context
        snprintf(kb->context, sizeof(kb->context), "%s", _context_to_str(b->context));

        g_tuiState.helpBindingCount++;
    }

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
    if (!g_HelpInitialized) InitHelpTableDef();
    return tui_DataTableHandleInput(&g_tuiState.helpTableState, &g_ActiveHelpTableDef, NULL, key);
}
