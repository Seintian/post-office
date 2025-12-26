/**
 * @file ui.h
 * @brief High-level UI components and application framework
 *
 * This file provides the main application window, screen management, and
 * high-level UI components for building complete terminal applications.
 */
#ifndef TUI_UI_H
#define TUI_UI_H

#include <stdbool.h> // for bool
#include <stddef.h>  // for size_t

#include "layout.h" // Includes tui_layout_manager_t
#include "widgets.h"

// Forward declarations for widget structures
typedef struct tui_widget_t tui_widget_t;
typedef struct tui_menu_bar_t tui_menu_bar_t;
typedef struct tui_status_bar_t tui_status_bar_t;
typedef struct tui_label_t tui_label_t;
typedef struct tui_button_t tui_button_t;
typedef struct tui_input_field_t tui_input_field_t;
typedef struct tui_list_t tui_list_t;
typedef struct tui_panel_t tui_panel_t;
typedef struct tui_progress_bar_t tui_progress_bar_t;
typedef struct tui_tab_container_t tui_tab_container_t;
typedef struct tui_tab_t tui_tab_t;
typedef struct tui_scroll_view_t tui_scroll_view_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Application window structure
 *
 * Represents a top-level application window that contains a menu bar,
 * status bar, and content area. Windows can also display modal dialogs.
 */
typedef struct tui_window_t {
    tui_widget_t *root;
    tui_menu_bar_t *menu_bar;
    tui_status_bar_t *status_bar;
    tui_widget_t *content;
    tui_widget_t *active_dialog;
    bool running;
    tui_theme_t current_theme;
} tui_window_t;

/** @name Window Management */
/** @{ */

/**
 * @brief Create a new application window
 *
 * @param title The window title to display in the title bar
 * @return Pointer to the new window, or NULL on failure
 */
tui_window_t *tui_window_create(const char *title);

/**
 * @brief Destroy a window and free its resources
 *
 * @param window The window to destroy
 */
void tui_window_destroy(tui_window_t *window);

/**
 * @brief Set the window size
 *
 * @param window The window to update
 * @param width The new width in character cells
 * @param height The new height in character cells
 */
void tui_window_set_size(tui_window_t *window, int width, int height);

/**
 * @brief Set the window position
 *
 * @param window The window to update
 * @param x The new x-coordinate (column) of the top-left corner
 * @param y The new y-coordinate (row) of the top-left corner
 */
void tui_window_set_position(tui_window_t *window, int x, int y);

/**
 * @brief Set the window title
 *
 * @param window The window to update
 * @param title  The new title text
 */
void tui_window_set_title(tui_window_t *window, const char *title);

/**
 * @brief Set the window content widget
 *
 * @param window  The window to update
 * @param content The widget to use as the main content
 */
void tui_window_set_content(tui_window_t *window, tui_widget_t *content);

/**
 * @brief Set the window menu bar
 *
 * @param window    The window to update
 * @param menu_bar  The menu bar to use
 */
void tui_window_set_menu_bar(tui_window_t *window, tui_menu_bar_t *menu_bar);

/**
 * @brief Set the window status bar
 *
 * @param window      The window to update
 * @param status_bar  The status bar to use
 */
void tui_window_set_status_bar(tui_window_t *window, tui_status_bar_t *status_bar);

/**
 * @brief Set the window close handler
 *
 * @param window The window to update
 * @param callback The function to call when the window is closed
 * @param user_data User data to pass to the callback
 */
typedef bool (*tui_window_close_callback_t)(tui_window_t *window, void *user_data);
void tui_window_set_close_callback(tui_window_t *window, tui_window_close_callback_t callback,
                                   void *user_data);

/**
 * @brief Show the window
 *
 * @param window The window to show
 */
void tui_window_show(tui_window_t *window);

/**
 * @brief Set the layout manager for the window
 *
 * @param window The window to update
 * @param layout The layout manager to use
 */
void tui_window_set_layout(tui_window_t *window, tui_layout_manager_t *layout);

/** @} */

/** @name Dialog and Window Operations */
/** @{ */

/**
 * @brief Show a modal dialog
 *
 * @param window The parent window
 * @param dialog The dialog widget to show
 */
void tui_dialog_show(tui_window_t *window, tui_widget_t *dialog);

/**
 * @brief Close the currently active dialog
 *
 * @param window The window containing the dialog
 */
void tui_dialog_close(tui_window_t *window);

/**
 * @brief Run the main event loop
 *
 * @param window The main application window
 */
void tui_window_run(tui_window_t *window);

/**
 * @brief Exit the main event loop
 *
 * @param window The window to exit from
 */
void tui_window_exit(tui_window_t *window);

/**
 * @brief Redraw the entire window
 *
 * @param window The window to redraw
 */
void tui_window_redraw(tui_window_t *window);

/**
 * @brief Get the currently active window
 *
 * @return The active window, or NULL if none
 */
tui_window_t *tui_window_get_active(void);

/**
 * @brief Set the active window
 *
 * @param window The window to make active
 */
void tui_window_set_active(tui_window_t *window);

/**
 * @brief Show a message box
 *
 * @param parent  Parent window (can be NULL)
 * @param title   Dialog title
 * @param message Message text to display
 */
void tui_message_box_show(tui_window_t *parent, const char *title, const char *message);

/**
 * @brief Show a confirmation dialog
 *
 * @param parent  Parent window (can be NULL)
 * @param title   Dialog title
 * @param message Confirmation message
 * @return true if user confirmed, false otherwise
 */
bool tui_confirm_box_show(tui_window_t *parent, const char *title, const char *message);

/**
 * @brief Show an input dialog
 *
 * @param parent       Parent window (can be NULL)
 * @param title        Dialog title
 * @param message      Prompt message
 * @param default_text Default text for the input field
 * @return User input as a string (must be freed by caller), or NULL if cancelled
 */
char *tui_input_box_show(tui_window_t *parent, const char *title, const char *message,
                         const char *default_text);

/** @} */

/** @name File Dialogs */
/** @{ */

/**
 * @brief Show an open file dialog
 *
 * @param parent Parent window (can be NULL)
 * @param title  Dialog title
 * @param filter File filter string (e.g., "Text Files (*.txt)")
 * @return Selected file path (must be freed by caller), or NULL if cancelled
 */
char *tui_file_dialog_open(tui_window_t *parent, const char *title, const char *filter);

/**
 * @brief Show a save file dialog
 *
 * @param parent      Parent window (can be NULL)
 * @param title       Dialog title
 * @param default_name Default filename
 * @param filter      File filter string (e.g., "Text Files (*.txt)")
 * @return Selected file path (must be freed by caller), or NULL if cancelled
 */
char *tui_file_dialog_save(tui_window_t *parent, const char *title, const char *default_name,
                           const char *filter);

/** @} */

/** @name Theme Management */
/** @{ */

/**
 * @brief Initialize a theme with default values
 *
 * @param theme Pointer to the theme structure to initialize
 */
void tui_theme_init(tui_theme_t *theme);

/**
 * @brief Set the current theme for a window
 *
 * @param window The window to update
 * @param theme  The theme to apply
 */
void tui_window_set_theme(tui_window_t *window, const tui_theme_t *theme);

/**
 * @brief Get the current theme for a window
 *
 * @param window The window to query
 * @return The current theme
 */
const tui_theme_t *tui_window_get_theme(const tui_window_t *window);

/** @} */

/** @name Screen Management */
/** @{ */

/**
 * @brief Opaque screen handle
 */
typedef struct tui_screen_t tui_screen_t;

/**
 * @brief Create a new screen
 *
 * @param content The root widget for this screen
 * @return New screen handle, or NULL on failure
 */
tui_screen_t *tui_screen_create(tui_widget_t *content);

/**
 * @brief Destroy a screen and free its resources
 *
 * @param screen The screen to destroy
 */
void tui_screen_destroy(tui_screen_t *screen);

/**
 * @brief Show a screen
 *
 * @param screen The screen to show
 */
void tui_screen_show(tui_screen_t *screen);

/**
 * @brief Hide a screen
 *
 * @param screen The screen to hide
 */
void tui_screen_hide(tui_screen_t *screen);

/**
 * @brief Set the screen title
 *
 * @param screen The screen to update
 * @param title  The new title
 */
void tui_screen_set_title(tui_screen_t *screen, const char *title);

/**
 * @brief Set the screen content
 *
 * @param screen  The screen to update
 * @param content The root widget for this screen
 */
void tui_screen_set_content(tui_screen_t *screen, tui_widget_t *content);

/**
 * @brief Navigate to a new screen
 *
 * @param screen The screen to navigate to
 */
void tui_screen_navigate_to(tui_screen_t *screen);

/**
 * @brief Navigate back to the previous screen
 */
void tui_screen_navigate_back(void);

/**
 * @brief Get the current screen
 *
 * @return The current screen, or NULL if none
 */
tui_screen_t *tui_screen_get_current(void);

/** @} */

/**
 * @brief Application class
 *
 * Represents the main application instance. Contains application metadata,
 * the main window, and lifecycle callbacks.
 */
typedef struct {
    const char *name;
    const char *version;
    tui_window_t *main_window;
    tui_screen_t *active_screen;
    void (*on_init)(void);
    void (*on_exit)(void);
} tui_application_t;

/** @name Application Lifecycle */
/** @{ */

/**
 * @brief Run the application
 *
 * @param app   Application instance
 * @param argc  Argument count
 * @param argv  Argument vector
 * @return Application exit code
 */
int tui_application_run(tui_application_t *app, int argc, char **argv);

/**
 * @brief Quit the application
 *
 * @param exit_code Exit code to return from tui_application_run()
 */
void tui_application_quit(int exit_code);

/** @} */

/** @name Widget Creation Helpers */
/** @{ */

/**
 * @brief Create a label widget
 *
 * @param text The label text
 * @param pos  The position of the label
 * @return New label widget
 */
tui_label_t *tui_label_create(const char *text, tui_point_t pos);

/**
 * @brief Create a button widget
 *
 * @param text     The button text
 * @param pos      The position of the button
 * @param size     The size of the button
 * @param callback Function to call when the button is clicked
 * @param user_data User data to pass to the callback
 * @return New button widget
 */
tui_button_t *tui_button_create_callback(const char *text, tui_point_t pos, tui_size_t size,
                                tui_button_callback_t callback, void *user_data);

/**
 * @brief Create a list widget
 *
 * @param bounds The bounds of the list
 * @return New list widget
 */
tui_list_t *tui_list_create(tui_rect_t bounds);

/**
 * @brief Create an input field widget
 *
 * @param bounds     The bounds of the input field
 * @param max_length Maximum input length
 * @return New input field widget
 */
tui_input_field_t *tui_input_field_create(tui_rect_t bounds, size_t max_length);

/**
 * @brief Create a panel widget
 *
 * @param bounds The bounds of the panel
 * @param title  The panel title (can be NULL)
 * @return New panel widget
 */
tui_panel_t *tui_panel_create(tui_rect_t bounds, const char *title);

/**
 * @brief Create a progress bar widget
 *
 * @param bounds    The bounds of the progress bar
 * @param min_value Minimum value
 * @param max_value Maximum value
 * @return New progress bar widget
 */
tui_progress_bar_t *tui_progress_bar_create(tui_rect_t bounds, float min_value, float max_value);

/**
 * @brief Create a tab container widget
 *
 * @param bounds The bounds of the tab container
 * @return New tab container widget
 */
tui_tab_container_t *tui_tab_container_create(tui_rect_t bounds);

/** @} */

/** @name Layout Helpers */
/** @{ */

/**
 * @brief Center a widget within its parent
 *
 * @param parent     The parent widget
 * @param child      The child widget to center
 * @param horizontal Whether to center horizontally
 * @param vertical   Whether to center vertically
 */
void tui_widget_center(tui_widget_t *parent, tui_widget_t *child, bool horizontal, bool vertical);

/**
 * @brief Align a widget within its parent
 *
 * @param parent   The parent widget
 * @param child    The child widget to align
 * @param h_align  Horizontal alignment
 * @param v_align  Vertical alignment
 */
void tui_widget_align(tui_widget_t *parent, tui_widget_t *child, tui_horizontal_alignment_t h_align,
                      tui_vertical_alignment_t v_align);

/**
 * @brief Make a widget fill its parent
 *
 * @param parent     The parent widget
 * @param child      The child widget to expand
 * @param horizontal Whether to fill horizontally
 * @param vertical   Whether to fill vertically
 */
void tui_widget_fill_parent(tui_widget_t *parent, tui_widget_t *child, bool horizontal,
                            bool vertical);

/** @} */

/** @name Keyboard Shortcuts */
/** @{ */

/**
 * @brief Keyboard shortcut definition
 */
typedef struct {
    int key;                  /**< Key code */
    const char *description;  /**< Description of what the shortcut does */
    void (*callback)(void *); /**< Function to call when shortcut is triggered */
    void *user_data;          /**< User data to pass to the callback */
} tui_shortcut_t;

/**
 * @brief Register keyboard shortcuts for a window
 *
 * @param window     The window to register shortcuts for
 * @param shortcuts  Array of shortcut definitions
 * @param count      Number of shortcuts in the array
 */
void tui_window_register_shortcuts(tui_window_t *window, const tui_shortcut_t *shortcuts,
                                   size_t count);

/**
 * @brief Unregister all keyboard shortcuts for a window
 *
 * @param window The window to unregister shortcuts from
 */
void tui_window_unregister_shortcuts(tui_window_t *window);

/** @} */

/** @name Tooltip Support */
/** @{ */

/**
 * @brief Show a tooltip at the specified screen position
 *
 * @param window     The parent window
 * @param text       The tooltip text
 * @param screen_pos Screen coordinates for the tooltip
 */
void tui_tooltip_show(tui_window_t *window, const char *text, tui_point_t screen_pos);

/**
 * @brief Hide the currently visible tooltip
 *
 * @param window The window that owns the tooltip
 */
void tui_tooltip_hide(tui_window_t *window);

/** @} */

/** @name Status Bar Helpers */
/** @{ */

/**
 * @brief Set the status bar text
 *
 * @param bar  The status bar
 * @param text The text to display
 */
void tui_status_bar_set_text(tui_status_bar_t *bar, const char *text);

/**
 * @brief Clear the status bar text
 *
 * @param bar The status bar to clear
 */
void tui_status_bar_clear(tui_status_bar_t *bar);

/**
 * @brief Push a message onto the status bar
 *
 * @param bar     The status bar
 * @param message The message to display
 * @param timeout_ms How long to show the message in milliseconds (0 for no timeout)
 */
void tui_status_bar_push_message(tui_status_bar_t *bar, const char *message, int timeout_ms);

/**
 * @brief Pop the current message from the status bar
 *
 * @param bar The status bar
 */
void tui_status_bar_pop_message(tui_status_bar_t *bar);

/** @} */

/** @name Common Dialogs */
/** @{ */

/**
 * @brief Show an about dialog
 *
 * @param parent Parent window (can be NULL)
 */
void tui_dialog_show_about(tui_window_t *parent);

/**
 * @brief Show a preferences dialog
 *
 * @param parent Parent window (can be NULL)
 */
void tui_dialog_show_preferences(tui_window_t *parent);

/**
 * @brief Show a help dialog
 *
 * @param parent Parent window (can be NULL)
 */
void tui_dialog_show_help(tui_window_t *parent);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // TUI_UI_H
