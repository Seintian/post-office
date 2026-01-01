#ifndef TUI_UIKIT_H
#define TUI_UIKIT_H

#include <stdbool.h>
#include <string.h>
#include <clay/clay.h>

/**
 * @brief Constructs a Clay_String from a dynamic C string (not a static literal).
 * Useful for runtime strings.
 */
#define CLAY_STRING_DYN(str) (Clay_String){ .length = (int)strlen(str), .chars = (str) }

/**
 * @file tui_uikit.h
 * @brief High-level atomic UI components ("English-ish" DSL).
 */

// --- Layout Macros ---

/**
 * @brief Begins a main content panel (fills available space).
 * Usage: TUI_PANEL_MAIN() { children... }
 */
#define TUI_PANEL_MAIN() CLAY(CLAY_ID("MainPanel"), \
    {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}, \
                .padding = {2, 2, 1, 1}, \
                .layoutDirection = CLAY_TOP_TO_BOTTOM}})

/**
 * @brief Begins a row layout container.
 * Usage: TUI_ROW(gap) { children... }
 */
#define TUI_ROW(gap_pixels) CLAY_AUTO_ID( \
    {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()}, \
                .childGap = gap_pixels, \
                .layoutDirection = CLAY_LEFT_TO_RIGHT}})

/**
 * @brief Begins a column layout container.
 * Usage: TUI_COLUMN(gap) { children... }
 */
#define TUI_COLUMN(gap_pixels) CLAY_AUTO_ID( \
    {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}, \
                .childGap = gap_pixels, \
                .layoutDirection = CLAY_TOP_TO_BOTTOM}})

// --- Atomic Widgets ---

/**
 * @brief Draws a standard text label.
 * @param text The string to display.
 * @param is_waring If true, renders in error/warning color.
 */
void tui_ui_label(const char* text, bool is_warning);

/**
 * @brief Draws a header text (larger/bold).
 * @param text The header string.
 */
void tui_ui_header(const char* text);

/**
 * @brief Draws a clickable button.
 * 
 * @param id Unique ID for the button.
 * @param label Text to display.
 * @param callback Click handler (Clay standard callback).
 * @param user_data Data passed to callback.
 * @param is_active If true, renders in "active/pressed" state style.
 */
void tui_ui_button(const char* id, const char* label, void (*callback)(Clay_ElementId, Clay_PointerData, void*), void* user_data, bool is_active);

/**
 * @brief Renders a standard keyboard shortcut hint (e.g., "[Q] Quit").
 * 
 * @param key_label The key (e.g., "Q").
 * @param action_label The action (e.g., "Quit").
 */
void tui_ui_shortcut_hint(const char* key_label, const char* action_label);

#endif // TUI_UIKIT_H
