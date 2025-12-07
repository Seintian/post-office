/**
 * @file tui.h
 * @brief Main TUI library interface
 * 
 * This file provides the core functionality for initializing and managing
 * the Terminal User Interface. It includes functions for creating windows,
 * handling input, and managing the application lifecycle.
 */

#ifndef TUI_H
#define TUI_H

#include "tui/types.h"
#include "tui/layout.h"
#include "tui/widgets.h"
#include "tui/table.h"
#include "tui/components.h"
#include "tui/ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the TUI system
 * 
 * This function must be called before any other TUI functions.
 * It initializes the terminal, sets up signal handlers, and prepares
 * the screen for TUI operations.
 * 
 * @return true if initialization was successful, false otherwise
 */
bool tui_init(void);

/**
 * @brief Cleanup and restore terminal state
 * 
 * Call this function before exiting your application to restore
 * the terminal to its original state.
 */
void tui_cleanup(void);

/**
 * @brief Set the target frames per second for the main event loop
 * 
 * This function sets the target frame rate for the main event loop.
 * The actual frame rate may vary depending on system performance.
 * 
 * @param fps Target frames per second (must be > 0)
 * @return bool True if the FPS was set successfully, false if the value is invalid
 */
bool tui_set_target_fps(int fps);

/**
 * @brief Run the main event loop
 * 
 * This function enters the main event processing loop. It will not return
 * until tui_quit() is called. The frame rate can be controlled using
 * tui_set_target_fps() before calling this function.
 */
void tui_run(void);

/**
 * @brief Stop the main event loop
 * 
 * Call this function to exit the main event loop and return from tui_run().
 */
void tui_quit(void);

/**
 * @brief Check if the TUI is running
 * @return true if running
 */
bool tui_is_running(void);

/**
 * @brief Get terminal dimensions
 * 
 * @return A tui_size_t structure containing the width and height of the terminal
 *         in character cells.
 */
tui_size_t tui_get_screen_size(void);

/**
 * @brief Set the root widget
 * 
 * The root widget is the top-level container that will be drawn first.
 * All other widgets should be children of this widget.
 * 
 * @param root The widget to use as the root container
 */
void tui_set_root(tui_widget_t* root);

/**
 * @brief Get the root widget
 * 
 * @return The root widget or NULL if not set
 */
tui_widget_t* tui_get_root(void);

/**
 * @brief Get the currently focused widget
 * 
 * @return A pointer to the currently focused widget, or NULL if no widget
 *         has focus.
 */
tui_widget_t* tui_get_focused_widget(void);

/**
 * @brief Process all pending events
 * 
 * This function processes all pending input and window events.
 * It should be called regularly from the main loop.
 */
void tui_process_events(void);

/**
 * @brief Render the current frame
 * 
 * Renders all visible windows and their contents.
 * This function should be called after processing events.
 */
void tui_render(void);

/**
 * @brief Sleep for a specified number of milliseconds
 * 
 * @param ms Number of milliseconds to sleep
 */
void tui_sleep(unsigned int ms);

/**
 * @brief Set the layout manager for a window
 * 
 * @param window The window to update
 * @param layout The layout manager to use
 */
void tui_window_set_layout(tui_window_t* window, tui_layout_manager_t* layout);

/**
 * @brief Set the focused widget
 * 
 * @param widget The widget to give focus to, or NULL to remove focus
 */
void tui_set_focus(tui_widget_t* widget);

/**
 * @brief Process a single event from the queue
 * 
 * This function processes one event from the input queue. It's useful
 * for custom event loops.
 * 
 * @return true if an event was processed, false if the queue was empty
 */
bool tui_process_event(void);

/**
 * @brief Force a redraw of the entire UI
 * 
 * Call this function when you need to update the display immediately.
 */
void tui_redraw(void);

/**
 * @brief Create a color pair
 * 
 * @param fg Foreground color (0-255)
 * @param bg Background color (0-255)
 * @return A tui_color_pair_t that can be used with tui_set_color()
 */
tui_color_pair_t tui_create_color_pair(int16_t fg, int16_t bg);

/**
 * @brief Set default color scheme
 * 
 * @param fg Default foreground color
 * @param bg Default background color
 */
void tui_set_default_colors(int16_t fg, int16_t bg);

/**
 * @brief Convert screen coordinates to widget coordinates
 * 
 * @param widget The target widget
 * @param screen_pos Screen coordinates to convert
 * @return The equivalent coordinates in the widget's coordinate system
 */
tui_point_t tui_screen_to_widget(const tui_widget_t* widget, tui_point_t screen_pos);

/**
 * @brief Convert widget coordinates to screen coordinates
 * 
 * @param widget The source widget
 * @param widget_pos Widget coordinates to convert
 * @return The equivalent screen coordinates
 */
tui_point_t tui_widget_to_screen(const tui_widget_t* widget, tui_point_t widget_pos);

/**
 * @name Widget Manipulation
 * Functions for modifying widget properties
 * @{
 */

/**
 * @brief Set a widget's position
 * 
 * @param widget The widget to modify
 * @param pos The new position (top-left corner)
 */
void tui_widget_set_position(tui_widget_t* widget, tui_point_t pos);

/**
 * @brief Set a widget's size
 * 
 * @param widget The widget to modify
 * @param size The new size in character cells
 */
void tui_widget_set_size(tui_widget_t* widget, tui_size_t size);

/**
 * @brief Set a widget's position and size
 * 
 * @param widget The widget to modify
 * @param bounds The new bounding rectangle
 */
void tui_widget_set_bounds(tui_widget_t* widget, tui_rect_t bounds);

/**
 * @brief Show or hide a widget
 * 
 * Hidden widgets are not drawn and do not receive events.
 * 
 * @param widget The widget to modify
 * @param visible true to show, false to hide
 */
void tui_widget_set_visible(tui_widget_t* widget, bool visible);

/**
 * @brief Enable or disable a widget
 * 
 * Disabled widgets are drawn but do not respond to input.
 * 
 * @param widget The widget to modify
 * @param enabled true to enable, false to disable
 */
void tui_widget_set_enabled(tui_widget_t* widget, bool enabled);

/**
 * @brief Set whether a widget can receive focus
 * 
 * @param widget The widget to modify
 * @param focusable true if the widget can receive focus
 */
void tui_widget_set_focusable(tui_widget_t* widget, bool focusable);

/**
 * @brief Add a child widget
 * 
 * @param parent The parent widget (must be a container)
 * @param child The widget to add as a child
 */
void tui_widget_add_child(tui_widget_t* parent, tui_widget_t* child);

/**
 * @brief Remove a child widget
 * 
 * @param parent The parent widget
 * @param child The child widget to remove
 */
void tui_widget_remove_child(tui_widget_t* parent, tui_widget_t* child);

/** @} */ // end of Widget Manipulation

/**
 * @name Event Handling
 * Functions for working with events
 * @{
 */

/**
 * @brief Send an event to a widget
 * 
 * @param widget The target widget
 * @param event The event to send
 * @return true if the event was handled, false otherwise
 */
bool tui_send_event(tui_widget_t* widget, const tui_event_t* event);

/**
 * @brief Post an event to the event queue
 * 
 * The event will be processed during the next event loop iteration.
 * 
 * @param event The event to post
 */
void tui_post_event(const tui_event_t* event);

/** @} */ // end of Event Handling

/**
 * @name Screen Management
 * Functions for working with screens
 * 
 * Screens represent different "pages" in your application. Only one screen
 * is active at a time.
 * @{
 */

typedef struct tui_screen_t tui_screen_t;

/**
 * @brief Create a new screen
 * 
 * @param root The root widget for this screen
 * @return A pointer to the new screen, or NULL on failure
 */
tui_screen_t* tui_screen_create(tui_widget_t* root);

/**
 * @brief Destroy a screen
 * 
 * @param screen The screen to destroy
 */
void tui_screen_destroy(tui_screen_t* screen);

/**
 * @brief Set the active screen
 * 
 * @param screen The screen to make active, or NULL to have no active screen
 */
void tui_screen_set_active(tui_screen_t* screen);

/**
 * @brief Get the currently active screen
 * 
 * @return The active screen, or NULL if no screen is active
 */
tui_screen_t* tui_screen_get_active(void);

/** @} */ // end of Screen Management

/**
 * @name Dialog Management
 * Functions for working with modal dialogs
 * @{
 */

/**
 * @brief Show a modal dialog
 * 
 * The dialog will be centered on screen and will capture all input.
 * 
 * @param dialog The dialog widget to show
 */
// Dialog management logic moved to ui.h to support window-based dialogs
// void tui_dialog_show(tui_widget_t* dialog);
// void tui_dialog_hide(void);
// tui_widget_t* tui_dialog_get_active(void);

/** @} */ // end of Dialog Management

/**
 * @name Theming
 * Functions for working with themes
 * @{
 */

/**
 * @brief Set the current theme
 * 
 * @param theme The theme to use
 */
void tui_theme_set(const tui_theme_t* theme);

/**
 * @brief Get the current theme
 * 
 * @return A pointer to the current theme
 */
const tui_theme_t* tui_theme_get(void);

/** @} */ // end of Theming

/**
 * @name Utility Functions
 * General-purpose utility functions
 * @{
 */

/**
 * @brief Draw a box with optional border
 * 
 * @param bounds The bounding rectangle
 * @param has_border Whether to draw a border
 */
void tui_draw_box(tui_rect_t bounds, bool has_border);

/**
 * @brief Draw text at the specified position
 * 
 * @param pos The position to draw at
 * @param text The text to draw
 * @param attrs Text attributes (bold, underline, etc.)
 */
void tui_draw_text(tui_point_t pos, const char* text, uint16_t attrs);

/**
 * @brief Draw centered text within a rectangle
 * 
 * @param bounds The bounding rectangle
 * @param text The text to draw
 * @param attrs Text attributes
 */
void tui_draw_text_centered(tui_rect_t bounds, const char* text, uint16_t attrs);

/** @} */ // end of Utility Functions

/**
 * @name Input Handling
 * Low-level input handling functions
 * @{
 */

/**
 * @brief Handle a key press
 * 
 * @param key The key that was pressed
 * @return true if the key was handled, false otherwise
 */
bool tui_handle_key(int key);

/**
 * @brief Handle a mouse event
 * 
 * @param x The mouse X coordinate
 * @param y The mouse Y coordinate
 * @param button The mouse button state
 * @param pressed Whether the button was pressed (true) or released (false)
 * @return true if the event was handled, false otherwise
 */
bool tui_handle_mouse(int x, int y, uint32_t button, bool pressed);

/** @} */ // end of Input Handling

#ifdef __cplusplus
} // extern "C"
#endif

#endif // TUI_H
