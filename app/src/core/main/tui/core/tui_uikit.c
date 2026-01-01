#include "tui_uikit.h"
#include <clay/clay.h>
#include <renderer/clay_ncurses_renderer.h>
#include <string.h>

/**
 * @file tui_uikit.c
 * @brief Implementation of atomic UI components.
 */

// Constants (internal) - Renamed to avoid conflicts
#define TUI_COLOR_WHITE (Clay_Color){255, 255, 255, 255}
#define TUI_COLOR_DIM   (Clay_Color){120, 120, 120, 255}
#define TUI_COLOR_ACCENT (Clay_Color){100, 200, 255, 255}
#define TUI_COLOR_WARN  (Clay_Color){255, 100, 100, 255}

// tui_ui_begin/end functions removed (replaced by macros in header)

void tui_ui_label(const char* text, bool is_warning) {
    Clay_Color col = is_warning ? TUI_COLOR_WARN : TUI_COLOR_WHITE;
    uint32_t id_seed = (uint32_t)(uintptr_t)text;

    CLAY(CLAY_IDI("LabelWrapper", id_seed), 
         {.layout = {.sizing = {.width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT()}}}) {

        CLAY_TEXT(CLAY_STRING_DYN((char*)text), 
                  CLAY_TEXT_CONFIG({.textColor = col, .fontId = 0}));
    }
}

void tui_ui_header(const char* text) {
    uint32_t id_seed = (uint32_t)(uintptr_t)text;

    CLAY(CLAY_IDI("HeaderWrapper", id_seed),
        {.layout = {.sizing = {.width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT()},
                    .padding = {0, 0, 0, 1}}}) { 

        CLAY_TEXT(CLAY_STRING_DYN((char*)text), 
                  CLAY_TEXT_CONFIG({.textColor = TUI_COLOR_ACCENT, .fontId = CLAY_NCURSES_FONT_BOLD}));
    }
}

void tui_ui_button(const char* id, const char* label, void (*callback)(Clay_ElementId, Clay_PointerData, void*), void* user_data, bool is_active) {
    Clay_String idStr = (Clay_String){.length = (int)strlen(id), .chars = id};
    Clay_ElementId elementId = Clay_GetElementId(idStr);

    CLAY(elementId, 
        {.layout = {.padding = {2, 2, 1, 1}},
         .border = {.width = {2, 2, 2, 2, 0}, .color = is_active ? TUI_COLOR_ACCENT : TUI_COLOR_DIM}}) {

        Clay_Ncurses_OnClick(callback, user_data);

        bool hovered = Clay_Hovered();
        Clay_Color textCol = (hovered || is_active) ? TUI_COLOR_ACCENT : TUI_COLOR_DIM;

        CLAY_TEXT(CLAY_STRING_DYN((char*)label), 
                  CLAY_TEXT_CONFIG({.textColor = textCol, .fontId = is_active ? CLAY_NCURSES_FONT_BOLD : 0}));
    }
}

void tui_ui_shortcut_hint(const char* key_label, const char* action_label) {
    uint32_t id_seed = (uint32_t)(uintptr_t)key_label;

    CLAY(CLAY_IDI("Shortcut", id_seed), 
        {.layout = {.sizing = {.width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT()},
                    .childGap = 1,
                    .padding = {1, 1, 0, 0}}}) {

        // Key [Q]
        CLAY_TEXT(CLAY_STRING_DYN((char*)key_label), 
                  CLAY_TEXT_CONFIG({.textColor = TUI_COLOR_ACCENT, .fontId = CLAY_NCURSES_FONT_BOLD}));

        // Action "Quit"
        CLAY_TEXT(CLAY_STRING_DYN((char*)action_label), 
                  CLAY_TEXT_CONFIG({.textColor = TUI_COLOR_DIM}));
    }
}
