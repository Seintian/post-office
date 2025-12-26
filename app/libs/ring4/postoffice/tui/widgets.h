/**
 * @file widgets.h
 * @brief Widget definitions for the TUI library
 *
 * This file contains the implementation of various UI widgets that can be used
 * to build terminal-based user interfaces. Widgets are the basic building blocks
 * of the TUI system.
 */

#ifndef TUI_WIDGETS_H
#define TUI_WIDGETS_H

#include <stddef.h> // for size_t

#include "layout.h" // Includes tui_layout_params_t and layout managers
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for widget structures
typedef struct tui_container_t tui_container_t;
typedef struct tui_label_t tui_label_t;
typedef struct tui_panel_t tui_panel_t;
typedef struct tui_progress_bar_t tui_progress_bar_t;
typedef struct tui_input_field_t tui_input_field_t;
typedef struct tui_status_bar_t tui_status_bar_t;
typedef struct tui_menu_t tui_menu_t;
typedef struct tui_menu_item_t tui_menu_item_t;
typedef struct tui_tab_container_t tui_tab_container_t;
typedef struct tui_tab_t tui_tab_t;
typedef struct tui_scroll_view_t tui_scroll_view_t;

typedef struct tui_progress_bar_style_t {
    char fill_char;       /**< Character used to fill the progress bar */
    char empty_char;      /**< Character used for the empty part of the bar */
    bool show_percentage; /**< Whether to show the percentage text */
    char format[32];      /**< Format string for the progress text (e.g., "%.1f%%") */
} tui_progress_bar_style_t;

// Callback types (moved to types.h)

// Base widget structure is defined in types.h

// Layout parameters are defined in layout.h

// Container widget base structure
typedef struct tui_container_t {
    tui_widget_t base;            /**< Base widget properties */
    tui_widget_t *first_child;    /**< First child widget */
    tui_widget_t *last_child;     /**< Last child widget */
    int child_count;              /**< Number of child widgets */
    tui_layout_manager_t *layout; /**< Layout manager for this container */
    bool auto_delete_children;    /**< Whether to delete children when container is destroyed */
} tui_container_t;

// Button widget
struct tui_button_t {
    tui_widget_t base;                    /**< Base widget properties */
    char *text;                           /**< Button text */
    tui_button_click_callback_t callback; /**< Click callback */
    bool is_default;                      /**< Whether this is the default button */
    bool is_cancel;                       /**< Whether this is the cancel button */
    bool is_pressed;                      /**< Whether the button is currently pressed */
    tui_button_click_callback_t on_right_click; /**< Right click callback */
    void (*on_scroll)(struct tui_button_t* btn, int delta, void* user_data); /**< Scroll callback (delta > 0 up, < 0 down) */
};

// Label widget
struct tui_label_t {
    tui_widget_t base;                    /**< Base widget properties */
    char *text;                           /**< Label text */
    tui_horizontal_alignment_t alignment; /**< Text alignment */
    bool word_wrap;                       /**< Whether to wrap long lines */
    bool auto_size;                       /**< Whether to automatically size to content */
};

// Panel widget
struct tui_panel_t {
    struct tui_container_t base; /**< Base container properties */
    char *title;                 /**< Optional panel title */
    int border_style;            /**< Border style (0=none, 1=single, 2=double) */
    bool show_border;            /**< Whether to show the border */
};

// Progress bar widget
struct tui_progress_bar_t {
    tui_widget_t base;              /**< Base widget properties */
    float min_value;                /**< Minimum value */
    float max_value;                /**< Maximum value */
    tui_progress_bar_style_t style; /**< Progress bar style */
    bool show_text;                 /**< Whether to show the value as text */
};

// List widget item
struct tui_list_item_t {
    char *text;      /**< Item text */
    void *user_data; /**< User data associated with this item */
    bool selected;   /**< Whether this item is selected */
    bool disabled;   /**< Whether this item is disabled */
};

struct tui_list_t {
    tui_widget_t base;                    /**< Base widget properties */
    struct tui_list_item_t **items;       /**< Array of list items */
    int item_count;                       /**< Number of items in the list */
    int selected_index;                   /**< Index of the selected item (-1 if none) */
    int top_visible;                      /**< Index of the top visible item */
    int visible_items;                    /**< Number of visible items */
    bool multi_select;                    /**< Whether multiple selection is enabled */
    tui_list_select_callback_t on_select; /**< Selection callback */
};

// Input field widget
struct tui_input_field_t {
    tui_widget_t base;  /**< Base widget properties */
    char *text;         /**< Current text content */
    int cursor_pos;     /**< Current cursor position */
    int scroll_offset;  /**< Horizontal scroll offset */
    int max_length;     /**< Maximum input length (0 for unlimited) */
    bool password_mode; /**< Whether to hide input (for passwords) */
    bool read_only;     /**< Whether the field is read-only */
};

// Status bar widget
struct tui_status_bar_t {
    tui_widget_t base;           /**< Base widget properties */
    char *left_text;             /**< Text to display on the left side */
    char *right_text;            /**< Text to display on the right side */
    struct tui_list_t *messages; /**< List of status messages */
    int message_timeout;         /**< Default message timeout in ms */
};

// Menu item structure
struct tui_menu_item_t {
    char *text;                 /**< Menu item text */
    char *shortcut;             /**< Keyboard shortcut text (NULL for none) */
    int modifiers;              /**< Modifier keys bitmask for the shortcut */
    struct tui_menu_t *submenu; /**< Submenu (for nested menus) */
    tui_callback_t callback;    /**< Callback function */
    void *user_data;            /**< User data for the callback */
    bool enabled;               /**< Whether the menu item is enabled */
    bool checked;               /**< Whether to show a checkmark */
    bool is_checkable;          /**< Whether this is a checkable item */
    bool is_separator;          /**< Whether this is a separator */
};

// Menu structure
struct tui_menu_t {
    char *title;                    /**< Menu title */
    struct tui_menu_item_t **items; /**< Array of menu items */
    int item_count;                 /**< Number of menu items */
    tui_rect_t bounds;              /**< Screen bounds when menu is open */
    bool is_open;                   /**< Whether the menu is currently open */
};

// Menu bar structure
struct tui_menu_bar_t {
    tui_widget_t base;         /**< Base widget properties */
    struct tui_menu_t **menus; /**< Array of menus */
    int menu_count;            /**< Number of menus */
    int active_menu;           /**< Index of the active menu (-1 if none) */
};

// Tab structure
struct tui_tab_t {
    char *title;           /**< Tab title */
    tui_widget_t *content; /**< Tab content widget */
    bool enabled;          /**< Whether the tab is enabled */
};

// Tab container widget
struct tui_tab_container_t {
    struct tui_container_t base; /**< Base container properties */
    struct tui_tab_t **tabs;     /**< Array of tabs */
    int tab_count;               /**< Number of tabs */
    int selected_tab;            /**< Index of the selected tab */
    int tab_position;            /**< Position of the tab bar (0=top, 1=right, 2=bottom, 3=left) */
};

// Scroll view widget
struct tui_scroll_view_t {
    struct tui_container_t base; /**< Base container properties */
    tui_point_t scroll_offset;   /**< Current scroll position */
    tui_size_t content_size;     /**< Size of the scrollable content */
    bool show_h_scrollbar;       /**< Whether to show the horizontal scrollbar */
    bool show_v_scrollbar;       /**< Whether to show the vertical scrollbar */
    bool auto_hide_scrollbars;   /**< Whether to hide scrollbars when not needed */
};

// Checkbox widget
struct tui_checkbox_t {
    tui_widget_t base; /**< Base widget properties */
    char *text;        /**< Checkbox label */
    bool checked;      /**< Whether the checkbox is checked */
    bool tristate;     /**< Whether to support a third 'indeterminate' state */
    int state;         /**< Checkbox state (0=unchecked, 1=checked, 2=indeterminate) */
    tui_checkbox_toggle_callback_t on_toggle; /**< Toggle callback */
};

struct tui_checkbox_t* tui_checkbox_create(const char* text, tui_rect_t bounds);
void tui_checkbox_set_toggle_callback(struct tui_checkbox_t* checkbox, tui_checkbox_toggle_callback_t callback, void* user_data);

// Radio button widget
struct tui_radio_button_t {
    tui_widget_t base;                            /**< Base widget properties */
    char *text;                                   /**< Radio button label */
    bool selected;                                /**< Whether this radio button is selected */
    int group_id;                                 /**< Group ID for mutual exclusion */
    tui_radio_button_select_callback_t on_select; /**< Selection callback */
};

struct tui_radio_button_t* tui_radio_button_create(const char* text, tui_rect_t bounds, int group_id);
void tui_radio_button_set_select_callback(struct tui_radio_button_t* radio, tui_radio_button_select_callback_t callback, void* user_data);

// Combo box widget
struct tui_combo_box_t {
    tui_widget_t base;                         /**< Base widget properties */
    char **items;                              /**< Array of item strings */
    int item_count;                            /**< Number of items */
    int selected_index;                        /**< Index of the selected item (-1 if none) */
    bool is_dropdown_open;                     /**< Whether the dropdown is currently open */
    tui_combo_box_select_callback_t on_select; /**< Selection callback */
};

// Slider widget
struct tui_slider_t {
    tui_widget_t base;                      /**< Base widget properties */
    float min_value;                        /**< Minimum value */
    float max_value;                        /**< Maximum value */
    float value;                            /**< Current value */
    float step;                             /**< Step size for value changes */
    bool show_value;                        /**< Whether to show the current value */
    tui_orientation_t orientation;          /**< Slider orientation */
    tui_slider_change_callback_t on_change; /**< Value change callback */
};

// Progress bar style structure is already defined above

/**
 * @name Base Widget Functions
 * Functions that operate on the base Widget type
 * @{
 */

/**
 * @brief Initialize a widget with the specified type
 *
 * @param widget Pointer to the widget to initialize
 * @param type The type of widget to create
 */
void tui_widget_init(struct tui_widget_t *widget, tui_widget_type_t type);

/**
 * @brief Free resources associated with a widget
 *
 * @param widget The widget to destroy
 */
void tui_widget_destroy(struct tui_widget_t *widget);

/**
 * @brief Update the widget's state
 *
 * @param widget The widget to update
 */
void tui_widget_update(struct tui_widget_t *widget);

/**
 * @brief Draw the widget on the screen
 *
 * @param widget The widget to draw
 */
void tui_widget_draw(struct tui_widget_t *widget);

/**
 * @brief Handle an event for the widget
 *
 * @param widget The widget to handle the event for
 * @param event The event to handle
 * @return true if the event was handled, false otherwise
 */
bool tui_widget_handle_event(struct tui_widget_t *widget, const tui_event_t *event);

/**
 * @brief Check if a point is within the widget's bounds
 *
 * @param widget The widget to check
 * @param point The point to check (in screen coordinates)
 * @return true if the point is within the widget, false otherwise
 */
bool tui_widget_contains_point(const struct tui_widget_t *widget, tui_point_t point);

/**
 * @brief Find the topmost widget at the specified point
 *
 * @param widget The root widget to search from
 * @param point The point to search at (in screen coordinates)
 * @return The topmost widget at the point, or NULL if none found
 */
struct tui_widget_t *tui_widget_find_at(struct tui_widget_t *widget, tui_point_t point);

/** @} */ // end of Base Widget Functions

/**
 * @name Button Widget
 * A clickable button widget
 * @{
 */

/**
 * @brief Create a new button widget
 *
 * @param text The button text
 * @param bounds The position and size of the button
 * @return A pointer to the new button, or NULL on failure
 */
struct tui_button_t *tui_button_create(const char *text, tui_rect_t bounds);

/**
 * @brief Set the button's click callback
 *
 * @param button The button to update
 * @param callback The function to call when the button is clicked
 * @param user_data User data to pass to the callback
 */
void tui_button_set_click_callback(struct tui_button_t *button,
                                   tui_button_click_callback_t callback, void *user_data);

/**
 * @brief Set the button's right-click callback
 * @param button The button
 * @param callback The callback function
 * @param user_data User data
 */
void tui_button_set_right_click_callback(struct tui_button_t *button,
                                         tui_button_click_callback_t callback, void *user_data);

/**
 * @brief Set the button's scroll callback
 * @param button The button
 * @param callback The callback function (delta +1 for up, -1 for down)
 * @param user_data User data
 */
void tui_button_set_scroll_callback(struct tui_button_t *button,
                                    void (*callback)(struct tui_button_t*, int, void*), void *user_data);

/**
 * @brief Set the button's text
 *
 * @param button The button to update
 * @param text The new button text
 */
void tui_button_set_text(struct tui_button_t *button, const char *text);

/**
 * @brief Get the button's text
 *
 * @param button The button to query
 * @return The current button text, or NULL if none is set
 */
const char *tui_button_get_text(const struct tui_button_t *button);

/**
 * @brief Free a button widget
 *
 * @param button The button to destroy
 */
void tui_button_destroy(struct tui_button_t *button);

/** @} */ // end of Button Widget

/**
 * @name Label Widget
 * A widget that displays static text
 * @{
 */

/**
 * @brief Create a new label widget
 *
 * @param text The text to display (can be NULL)
 * @param pos The position of the label
 * @return A pointer to the new label, or NULL on failure
 */
struct tui_label_t *tui_label_create(const char *text, tui_point_t pos);

/**
 * @brief Set the label's text
 *
 * @param label The label to update
 * @param text The new text to display (can be NULL)
 */
void tui_label_set_text(struct tui_label_t *label, const char *text);

/**
 * @brief Get the label's text
 *
 * @param label The label to query
 * @return The current text, or NULL if none is set
 */
const char *tui_label_get_text(const struct tui_label_t *label);

/**
 * @brief Set the text alignment
 *
 * @param label The label to update
 * @param align The new text alignment
 */
void tui_label_set_alignment(struct tui_label_t *label, tui_horizontal_alignment_t align);

/**
 * @brief Set whether the label should automatically size to fit its text
 *
 * @param label The label to update
 * @param auto_size Whether to enable auto-sizing
 */
void tui_label_set_auto_size(struct tui_label_t *label, bool auto_size);

/**
 * @brief Free a label widget
 *
 * @param label The label to destroy
 */
void tui_label_destroy(struct tui_label_t *label);

/** @} */ // end of Label Widget

/**
 * @name Panel Widget
 * A container widget with a border and optional title
 * @{
 */

/**
 * @brief Create a new panel widget
 *
 * @param bounds The position and size of the panel
 * @param title The panel title (can be NULL)
 * @return A pointer to the new panel, or NULL on failure
 */
struct tui_panel_t *tui_panel_create(tui_rect_t bounds, const char *title);

/**
 * @brief Set the panel's title
 *
 * @param panel The panel to update
 * @param title The new title (can be NULL)
 */
void tui_panel_set_title(struct tui_panel_t *panel, const char *title);

/**
 * @brief Get the panel's title
 *
 * @param panel The panel to query
 * @return The current title, or NULL if none is set
 */
const char *tui_panel_get_title(const struct tui_panel_t *panel);

/**
 * @brief Set whether the panel should have a border
 *
 * @param panel The panel to update
 * @param has_border Whether to show the border
 */
void tui_panel_set_border(struct tui_panel_t *panel, bool has_border);

/**
 * @brief Free a panel widget
 *
 * @param panel The panel to destroy
 */
void tui_panel_destroy(struct tui_panel_t *panel);

/** @} */ // end of Panel Widget

/**
 * @name List Widget
 * A scrollable list of selectable items
 * @{
 */

/**
 * @brief Create a new list widget
 *
 * @param bounds The position and size of the list
 * @return A pointer to the new list, or NULL on failure
 */
struct tui_list_t *tui_list_create(tui_rect_t bounds);

/**
 * @brief Add an item to the list
 *
 * @param list The list to add to
 * @param item The item text (will be copied)
 * @return true on success, false on failure
 */
bool tui_list_add_item(struct tui_list_t *list, const char *item);

/**
 * @brief Get an item from the list
 *
 * @param list The list to query
 * @param index The index of the item to get
 * @return The item text, or NULL if index is out of bounds
 */
const char *tui_list_get_item(const struct tui_list_t *list, int index);

/**
 * @brief Set the selection callback
 *
 * @param list The list to update
 * @param callback The function to call when an item is selected
 * @param user_data User data to pass to the callback
 */
void tui_list_set_select_callback(struct tui_list_t *list, tui_list_select_callback_t callback,
                                  void *user_data);

/**
 * @brief Get the index of the currently selected item
 *
 * @param list The list to query
 * @return The index of the selected item, or -1 if no item is selected
 */
int tui_list_get_selected_index(const struct tui_list_t *list);

/**
 * @brief Set the selected item by index
 *
 * @param list The list to update
 * @param index The index of the item to select, or -1 to clear the selection
 */
void tui_list_set_selected_index(struct tui_list_t *list, int index);

/**
 * @brief Free a list widget
 *
 * @param list The list to destroy
 */
void tui_list_destroy(struct tui_list_t *list);

/** @} */ // end of List Widget

/**
 * @name Container Widget
 * A widget that can contain other widgets
 * @{
 */

/**
 * @brief Create a new container widget
 *
 * @return A pointer to the new container, or NULL on failure
 */
struct tui_container_t *tui_container_create(void);

/**
 * @brief Add a child widget to the container
 *
 * @param container The container to add to
 * @param child The widget to add
 */
void tui_container_add(struct tui_container_t *container, struct tui_widget_t *child);

/**
 * @brief Remove a child widget from the container
 *
 * @param container The container to remove from
 * @param child The widget to remove
 */
void tui_container_remove(struct tui_container_t *container, struct tui_widget_t *child);

/**
 * @brief Remove all child widgets from the container
 *
 * @param container The container to clear
 */
void tui_container_remove_all(struct tui_container_t *container);

/**
 * @brief Set the layout manager for a container
 *
 * @param container The container to update
 * @param layout The layout manager to use (can be NULL)
 */
void tui_container_set_layout(struct tui_container_t *container, tui_layout_manager_t *layout);

/**
 * @brief Free a container widget and all its children
 *
 * @param container The container to destroy
 */
void tui_container_destroy(struct tui_container_t *container);

/** @} */ // end of Container Widget

/**
 * @name Progress Bar Widget
 * A visual indicator of progress
 * @{
 */

/**
 * @brief Create a new progress bar
 *
 * @param bounds The position and size of the progress bar
 * @param min_value Minimum value (typically 0.0)
 * @param max_value Maximum value (typically 100.0)
 * @return A pointer to the new progress bar, or NULL on failure
 */
struct tui_progress_bar_t *tui_progress_bar_create(tui_rect_t bounds, float min_value,
                                                   float max_value);

/**
 * @brief Set the range of the progress bar
 *
 * @param bar The progress bar to update
 * @param min_value Minimum value (typically 0.0)
 * @param max_value Maximum value (typically 100.0)
 */
void tui_progress_bar_set_range(struct tui_progress_bar_t *bar, float min_value, float max_value);

/**
 * @brief Set the current value of the progress bar
 *
 * @param bar The progress bar to update
 * @param value The new value (will be clamped between min_value and max_value)
 */
void tui_progress_bar_set_value(struct tui_progress_bar_t *bar, float value);

/**
 * @brief Get the current value of the progress bar
 *
 * @param bar The progress bar to query
 * @return The current value
 */
float tui_progress_bar_get_value(const struct tui_progress_bar_t *bar);

/**
 * @brief Set the format string for the progress bar text
 *
 * The format string should be a valid printf-style format string that accepts
 * a single float argument (the current progress as a percentage).
 * For example: "%.1f%%" would display as "42.5%"
 *
 * @param bar The progress bar to update
 * @param format The new format string (copied, max 31 chars + null terminator)
 */
void tui_progress_bar_set_format(struct tui_progress_bar_t *bar, const char *format);

/**
 * @brief Set the style of the progress bar
 *
 * @param bar The progress bar to update
 * @param style The style to apply
 */
void tui_progress_bar_set_style(struct tui_progress_bar_t *bar,
                                const struct tui_progress_bar_style_t *style);

/**
 * @brief Free a progress bar widget
 *
 * @param bar The progress bar to destroy
 */
void tui_progress_bar_destroy(struct tui_progress_bar_t *bar);

/** @} */ // end of Progress Bar Widget

/**
 * @name Tab Container Widget
 * A container with multiple tabbed pages
 * @{
 */

/**
 * @brief Create a new tab container
 *
 * @param bounds The position and size of the tab container
 * @return A pointer to the new tab container, or NULL on failure
 */
struct tui_tab_container_t *tui_tab_container_create(tui_rect_t bounds);

/**
 * @brief Add a new tab
 *
 * @param tabs The tab container to add to
 * @param title The tab's title (will be copied)
 * @param content The widget to display when this tab is selected
 * @return The index of the new tab, or -1 on failure
 */
int tui_tab_container_add_tab(struct tui_tab_container_t *tabs, const char *title,
                              struct tui_widget_t *content);

/**
 * @brief Remove a tab
 *
 * @param tabs The tab container to modify
 * @param index Index of the tab to remove
 */
void tui_tab_container_remove_tab(struct tui_tab_container_t *tabs, int index);

/**
 * @brief Select a tab by index
 *
 * @param tabs The tab container to update
 * @param index Index of the tab to select
 */
void tui_tab_container_set_selected_tab(struct tui_tab_container_t *tabs, int index);

/**
 * @brief Get the index of the currently selected tab
 *
 * @param tabs The tab container to query
 * @return The index of the selected tab, or -1 if no tabs exist
 */
int tui_tab_container_get_tab_count(const struct tui_tab_container_t *tabs);

/**
 * @brief Free a tab container and its contents
 *
 * @param tabs The tab container to destroy
{{ ... }}
 *
 * @param bounds The position and size of the input field
 * @param max_length Maximum number of characters that can be entered
 * @return A pointer to the new input field, or NULL on failure
 */
struct tui_input_field_t *tui_input_field_create(tui_rect_t bounds, size_t max_length);

/**
 * @brief Set the input field's text
 *
 * @param field The input field to update
 * @param text The new text (will be copied, can be NULL)
 */
void tui_input_field_set_max_length(struct tui_input_field_t *field, size_t max_length);

/**
 * @brief Get the input field's text
 *
 * @param field The input field to query
{{ ... }}
 * @brief Create a new status bar
 *
 * @param height Height of the status bar in rows (typically 1)
 * @return A pointer to the new status bar, or NULL on failure
 */
struct tui_status_bar_t *tui_status_bar_create(tui_rect_t bounds);

/**
 * @brief Set the status bar's text
 *
 * @param bar The status bar to update
 * @param text The text to display (will be copied, can be NULL)
 */
void tui_status_bar_set_left_text(struct tui_status_bar_t *bar, const char *text);

/**
 * @brief Push a message onto the status bar
 *
 * The message will be displayed temporarily, then the previous message will be restored.
 *
 * @param bar The status bar to update
 * @param message The message to display (will be copied)
 * @param timeout_ms How long to show the message in milliseconds (0 for no timeout)
 */
void tui_status_bar_show_message(struct tui_status_bar_t *bar, const char *message, int timeout_ms);

/**
 * @brief Pop the current message from the status bar
 *
 * Restores the previous message, if any.
 *
 * @param bar The status bar to update
 */
void tui_status_bar_clear_message(struct tui_status_bar_t *bar);

/**
 * @brief Clear the status bar's text
 */
void tui_status_bar_clear(struct tui_status_bar_t *bar);

/**
 * @name Menu Bar Widget
 * A horizontal menu bar with drop-down menus
 * @{
 */

/**
 * @brief Create a new menu bar
 *
 * @param bounds The position and size of the menu bar
 * @return A pointer to the new menu bar, or NULL on failure
 */
struct tui_menu_bar_t *tui_menu_bar_create(tui_rect_t bounds);

/**
 *
 * @param menu_bar The menu bar to add to
 * @return A handle to the new menu, or NULL on failure
 */
struct tui_menu_t *tui_menu_bar_add_menu(struct tui_menu_bar_t *menu_bar, const char *title);

/**
 * @brief Add an item to a menu
 *
 * @param menu The menu to add to
 * @param label The text to display (will be copied, can be NULL for separator)
 * @param shortcut Optional keyboard shortcut text (will be copied, can be NULL)
 * @param callback Function to call when the item is selected (can be NULL)
 * @param user_data User data to pass to the callback
 * @return true on success, false on failure
 */
bool tui_menu_add_item(struct tui_menu_t *menu, const char *label, const char *shortcut,
                       tui_callback_t callback, void *user_data);

/**
 * @brief Add a separator to a menu
 *
 * @param menu The menu to add to
 * @return true on success, false on failure
 */
bool tui_menu_add_separator(struct tui_menu_t *menu);

/**
 * @brief Free a menu bar and all its contents
 *
 * @param menu_bar The menu bar to destroy
 */
void tui_menu_bar_destroy(struct tui_menu_bar_t *menu_bar);

/** @} */ // end of Menu Bar Widget

/**
 * @name Dialog Functions
 * Pre-built dialog boxes for common user interactions
 * @{
 */

/**
 * @brief Display a message dialog
 *
 * @param parent The parent window (can be NULL)
 * @param title Dialog title (can be NULL)
 * @param message Message text to display
 */
void tui_dialog_show_message(tui_window_t *parent, const char *title, const char *message);

/**
 * @brief Display a confirmation dialog with Yes/No buttons
 *
 * @param parent The parent window (can be NULL)
 * @param title Dialog title (can be NULL)
 * @param message Message text to display
 * @return true if the user selected Yes, false otherwise
 */
bool tui_dialog_show_confirm(tui_window_t *parent, const char *title, const char *message);

/**
 * @brief Display a text input dialog
 *
 * @param parent The parent window (can be NULL)
 * @param title Dialog title (can be NULL)
 * @param prompt Message to display above the input field
 * @param default_text Default text for the input field (can be NULL)
 * @return The entered text (must be freed by the caller), or NULL if cancelled
 */
char *tui_dialog_show_input(tui_window_t *parent, const char *title, const char *prompt,
                            const char *default_text);

/** @} */ // end of Dialog Functions

#ifdef __cplusplus
}
#endif

#endif /* TUI_WIDGETS_H */
