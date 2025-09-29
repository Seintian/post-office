/**
 * @file types.h
 * @brief Core type definitions for the TUI library
 * 
 * This file contains the fundamental types used throughout the TUI library,
 * including basic geometric types, event handling, and theming.
 */

#ifndef TUI_TYPES_H
#define TUI_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations for widget structures
typedef struct tui_widget_t tui_widget_t;
typedef struct tui_window_t tui_window_t;
typedef struct tui_layout_manager_t tui_layout_manager_t;
typedef struct tui_button_t tui_button_t;
typedef struct tui_list_t tui_list_t;
typedef struct tui_checkbox_t tui_checkbox_t;
typedef struct tui_radio_button_t tui_radio_button_t;
typedef struct tui_combo_box_t tui_combo_box_t;
typedef struct tui_slider_t tui_slider_t;
typedef struct tui_event_t tui_event_t;

// Callback types
typedef void (*tui_button_click_callback_t)(tui_button_t* button, void* user_data);
typedef void (*tui_list_select_callback_t)(tui_list_t* list, int selected, void* user_data);
typedef void (*tui_checkbox_toggle_callback_t)(tui_checkbox_t* checkbox, bool checked, void* user_data);
typedef void (*tui_radio_button_select_callback_t)(tui_radio_button_t* radio, int selected_index, void* user_data);
typedef void (*tui_combo_box_select_callback_t)(tui_combo_box_t* combo_box, int selected_index, const char* selected_text, void* user_data);
typedef void (*tui_slider_change_callback_t)(tui_slider_t* slider, float value, void* user_data);
typedef void (*tui_callback_t)(void* user_data);

/**
 * @brief 2D point with integer coordinates
 * 
 * Represents a point in the terminal's coordinate system, where (0,0) is the top-left corner.
 * The x-coordinate increases to the right, and the y-coordinate increases downward.
 */
typedef struct tui_point_t {
    int16_t x; /**< Horizontal coordinate (0 = leftmost column) */
    int16_t y; /**< Vertical coordinate (0 = topmost row) */
} tui_point_t;

/**
 * @brief 2D size with integer dimensions
 * 
 * Represents the dimensions of a rectangular area in the terminal.
 */
typedef struct tui_size_t {
    int16_t width;  /**< Width in character cells */
    int16_t height; /**< Height in character cells */
} tui_size_t;

/**
 * @brief Rectangle defined by position and size
 * 
 * Represents a rectangular region in the terminal, commonly used for widget bounds.
 */
typedef struct tui_rect_t {
    tui_point_t position; /**< Top-left corner position */
    tui_size_t size;      /**< Dimensions of the rectangle */
} tui_rect_t;

/**
 * @brief Orientation for layout managers
 * 
 * Specifies the primary axis for laying out widgets in a container.
 */
typedef enum {
    TUI_ORIENTATION_HORIZONTAL, /**< Layout widgets left to right */
    TUI_ORIENTATION_VERTICAL    /**< Layout widgets top to bottom */
} tui_orientation_t;

/**
 * @brief Horizontal alignment options
 * 
 * Used to specify how content should be aligned within a container.
 */
typedef enum {
    TUI_ALIGN_LEFT,   /**< Align to the left edge */
    TUI_ALIGN_CENTER, /**< Center horizontally */
    TUI_ALIGN_RIGHT,  /**< Align to the right edge */
    TUI_ALIGN_FILL_H  /**< Fill available horizontal space */
} tui_horizontal_alignment_t;

/**
 * @brief Vertical alignment options
 * 
 * Used to specify how content should be aligned within a container.
 */
typedef enum {
    TUI_ALIGN_TOP,    /**< Align to the top edge */
    TUI_ALIGN_MIDDLE, /**< Center vertically */
    TUI_ALIGN_BOTTOM, /**< Align to the bottom edge */
    TUI_ALIGN_FILL_V  /**< Fill available vertical space */
} tui_vertical_alignment_t;

/**
 * @brief Color pair for terminal text and background
 * 
 * Represents a foreground/background color combination. The actual colors
 * are defined by the terminal's color palette.
 */
typedef uint16_t tui_color_pair_t;

/**
 * @brief Complete theme definition for the TUI
 * 
 * Contains all color combinations used throughout the interface.
 */
typedef struct tui_theme_t {
    tui_color_pair_t window_bg;         /**< Default window background */
    tui_color_pair_t window_fg;         /**< Default window foreground */
    tui_color_pair_t button_bg;         /**< Button background */
    tui_color_pair_t button_fg;         /**< Button text color */
    tui_color_pair_t button_active_bg;  /**< Button background when active/hovered */
    tui_color_pair_t button_active_fg;  /**< Button text color when active/hovered */
    tui_color_pair_t input_bg;          /**< Input field background */
    tui_color_pair_t input_fg;          /**< Input field text color */
    tui_color_pair_t input_active_bg;   /**< Input field background when focused */
    tui_color_pair_t input_active_fg;   /**< Input field text color when focused */
    tui_color_pair_t list_bg;           /**< List background */
    tui_color_pair_t list_fg;           /**< List text color */
    tui_color_pair_t list_selected_bg;  /**< Selected list item background */
    tui_color_pair_t list_selected_fg;  /**< Selected list item text color */
    tui_color_pair_t label_fg;          /**< Label text color */
    tui_color_pair_t title_fg;          /**< Title/header text color */
    tui_color_pair_t border;            /**< Border color */
    tui_color_pair_t highlight;         /**< Highlight/selection color */
    tui_color_pair_t error;             /**< Error message color */
    tui_color_pair_t warning;           /**< Warning message color */
    tui_color_pair_t success;           /**< Success message color */
} tui_theme_t;

/**
 * @brief Types of events that can be processed
 * 
 * Used to identify the type of event that occurred.
 */
typedef enum {
    TUI_EVENT_NONE,    /**< No event */
    TUI_EVENT_KEY,     /**< Keyboard event */
    TUI_EVENT_MOUSE,   /**< Mouse event */
    TUI_EVENT_RESIZE,  /**< Terminal resize event */
    TUI_EVENT_FOCUS,   /**< Focus change event */
    TUI_EVENT_CUSTOM   /**< Application-defined custom event */
} tui_event_type_t;

/**
 * @brief Event structure
 * 
 * Contains information about an event that occurred in the TUI.
 * The actual data stored depends on the event type.
 */
typedef struct tui_event_t {
    tui_event_type_t type; /**< Type of event that occurred */
    union {
        int key;    /**< Key code for keyboard events */
        struct {
            int x;          /**< Mouse X coordinate */
            int y;          /**< Mouse Y coordinate */
            uint32_t button; /**< Mouse button mask */
            bool pressed;    /**< True if button was pressed, false if released */
        } mouse;            /**< Mouse event data */
        struct {
            int width;      /**< New terminal width in characters */
            int height;     /**< New terminal height in characters */
        } resize;           /**< Resize event data */
        bool has_focus;     /**< True if widget gained focus, false if lost */
        void* custom_data;  /**< Application-defined data for custom events */
    } data; /**< Event-specific data */
} tui_event_t;

/**
 * @brief Button click callback function type
 * 
 * @param button The button that was clicked
 * @param user_data User data passed when the callback was registered
 */
typedef void (*tui_button_callback_t)(tui_button_t* button, void* user_data);

/**
 * @brief Event callback function type
 * 
 * @param widget The widget that received the event
 * @param event The event data
 * @param user_data User data passed when the callback was registered
 * @return bool True if the event was handled, false otherwise
 */
typedef bool (*tui_event_callback_t)(tui_widget_t* widget, const tui_event_t* event, void* user_data);

// Event handler structure
typedef struct tui_event_handler_t {
    tui_event_callback_t callback;
    void* user_data;
} tui_event_handler_t;

/**
 * @brief Function type for drawing a widget
 * 
 * @param widget The widget to draw
 */
typedef void (*tui_draw_function_t)(tui_widget_t* widget);

/**
 * @brief Type identifiers for widgets
 * 
 * Used to identify the specific type of a widget at runtime.
 */
typedef enum {
    TUI_WIDGET_BASE = 0,  /**< Base widget type */
    TUI_WIDGET_WINDOW,    /**< Top-level window */
    TUI_WIDGET_BUTTON,    /**< Clickable button */
    TUI_WIDGET_LABEL,     /**< Text label */
    TUI_WIDGET_INPUT,     /**< Text input field */
    TUI_WIDGET_LIST,      /**< Scrollable list */
    TUI_WIDGET_PANEL,     /**< Container with border and optional title */
    TUI_WIDGET_PROGRESS_BAR, /**< Progress indicator */
    TUI_WIDGET_CUSTOM     /**< Application-defined custom widget */
} tui_widget_type_t;

/**
 * @brief Base widget structure
 * 
 * All TUI widgets inherit from this base structure. Custom widgets should
 * include this as their first member to maintain compatibility.
 */
typedef struct tui_widget_t {
    tui_widget_type_t type;      /**< Type identifier for this widget */
    tui_rect_t bounds;          /**< Position and size of the widget */
    bool visible;               /**< Whether the widget is visible */
    bool enabled;               /**< Whether the widget is enabled */
    bool focusable;             /**< Whether the widget can receive focus */
    bool has_focus;             /**< Whether the widget currently has focus */
    void* user_data;            /**< Application-defined data */

    // Style
    tui_color_pair_t colors;    /**< Foreground/background colors */
    uint16_t attributes;        /**< Text attributes (bold, underline, etc.) */

    // Virtual function table
    tui_draw_function_t draw;   /**< Function to draw this widget */
    void (*free)(tui_widget_t* widget);   /**< Function to free widget resources */
    bool (*handle_event)(tui_widget_t* widget, const tui_event_t* event); /**< Event handler */

    // Event handling
    tui_event_handler_t* event_handlers;  /**< List of registered event handlers */
    size_t num_event_handlers;           /**< Number of registered event handlers */

    // Parent container (if any)
    tui_widget_t* parent;  /**< Parent widget (NULL for root widget) */
} tui_widget_t;

#endif /* TUI_TYPES_H */
