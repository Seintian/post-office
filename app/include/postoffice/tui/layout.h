/**
 * @file layout.h
 * @brief Layout management system for TUI widgets
 * 
 * This file defines the layout system used to position and size widgets within containers.
 * It includes layout managers, constraints, and utility functions for calculating widget
 * positions and sizes.
 */
#ifndef TUI_LAYOUT_H
#define TUI_LAYOUT_H

#include "types.h"  // Already includes the tui_orientation_t type

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Layout constraints for widgets
 * 
 * This structure defines how a widget should be sized and positioned within its parent container.
 * It includes minimum/maximum sizes, weights for flexible sizing, margins, padding,
 * and alignment properties.
 */
typedef struct {
    int min_width;
    int max_width;
    int min_height;
    int max_height;
    float weight_x;
    float weight_y;
    struct {
        int left;
        int top;
        int right;
        int bottom;
    } margin;
    struct {
        int left;
        int top;
        int right;
        int bottom;
    } padding;
    tui_horizontal_alignment_t h_align;
    tui_vertical_alignment_t v_align;
    bool expand_x;
    bool expand_y;
    bool fill_x;
    bool fill_y;
} tui_layout_params_t;

/**
 * @brief Layout manager interface
 * 
 * This structure defines the interface that all layout managers must implement.
 * It contains function pointers for layout, measurement, and cleanup operations.
 */
typedef struct tui_layout_manager_t {
    void (*layout)(struct tui_layout_manager_t* self, tui_widget_t* container);
    void (*measure)(struct tui_layout_manager_t* self, tui_widget_t* container, tui_size_t* size);
    void (*free)(struct tui_layout_manager_t* self);
    void* data;
} tui_layout_manager_t;

/** @name Built-in Layout Managers */
/** @{ */

/**
 * @brief Create a box layout manager
 * 
 * Arranges widgets in a single row or column with optional spacing between them.
 * 
 * @param orientation The orientation of the layout (TUI_ORIENTATION_HORIZONTAL or TUI_ORIENTATION_VERTICAL)
 * @param spacing Space between widgets in pixels
 * @return Newly created layout manager, or NULL on failure
 */
tui_layout_manager_t* tui_layout_box_create(tui_orientation_t orientation, int spacing);

/**
 * @brief Create a grid layout manager
 * 
 * Arranges widgets in a grid with the specified number of columns and rows.
 * 
 * @param rows Number of rows in the grid (use 0 for auto)
 * @param cols Number of columns in the grid (use 0 for auto, but at least one of rows or cols must be > 0)
 * @param h_spacing Horizontal spacing between columns
 * @param v_spacing Vertical spacing between rows
 * @return Newly created layout manager, or NULL on failure
 */
tui_layout_manager_t* tui_layout_grid_create(int rows, int cols, int h_spacing, int v_spacing);

/**
 * @brief Create a border layout manager
 * 
 * Arranges widgets in five regions: TUI_POSITION_NORTH, TUI_POSITION_SOUTH, TUI_POSITION_EAST, TUI_POSITION_WEST, and TUI_POSITION_CENTER.
 * 
 * @param spacing Space between regions in pixels
 * @return Newly created layout manager, or NULL on failure
 */
tui_layout_manager_t* tui_layout_border_create(int spacing);

/**
 * @brief Create a stack layout manager
 * 
 * Stacks widgets on top of each other, with the last widget on top.
 * 
 * @return Newly created layout manager, or NULL on failure
 */
tui_layout_manager_t* tui_layout_stack_create(void);
/** @} */

/** @name Layout Operations */
/** @{ */

/**
 * @brief Set the layout manager for a widget
 * 
 * @param widget The widget to set the layout for
 * @param layout The layout manager to use (can be NULL for no layout)
 */
void tui_widget_set_layout(tui_widget_t* widget, tui_layout_manager_t* layout);

/**
 * @brief Update the layout of a widget and its children
 * 
 * @param widget The widget to update
 */
void tui_widget_layout_update(tui_widget_t* widget);

/**
 * @brief Calculate the preferred size of a widget
 * 
 * @param widget The widget to measure
 * @param size   Output parameter for the preferred size
 */
void tui_widget_measure(tui_widget_t* widget, tui_size_t* size);
/** @} */

/** @name Constraint Helpers */
/** @{ */

/**
 * @brief Initialize layout parameters with default values
 * 
 * @param params The layout parameters to initialize
 */
void tui_layout_params_init(tui_layout_params_t* params);

/**
 * @brief Set the margins for a widget
 * 
 * @param params  The layout parameters to modify
 * @param left    Left margin in characters
 * @param top     Top margin in characters
 * @param right   Right margin in characters
 * @param bottom  Bottom margin in characters
 */
void tui_layout_params_set_margin(tui_layout_params_t* params, int left, int top, int right, int bottom);

/**
 * @brief Set the padding for a widget
 * 
 * @param params  The layout parameters to modify
 * @param left    Left padding in characters
 * @param top     Top padding in characters
 * @param right   Right padding in characters
 * @param bottom  Bottom padding in characters
 */
void tui_layout_params_set_padding(tui_layout_params_t* params, int left, int top, int right, int bottom);

/**
 * @brief Set the preferred size hint for a widget
 * 
 * @param params  The layout parameters to modify
 * @param width   Preferred width in characters
 * @param height  Preferred height in characters
 */
void tui_layout_params_set_size_hint(tui_layout_params_t* params, int width, int height);

/**
 * @brief Set the minimum size for a widget
 * 
 * @param params  The layout parameters to modify
 * @param width   Minimum width in characters
 * @param height  Minimum height in characters
 */
void tui_layout_params_set_min_size(tui_layout_params_t* params, int width, int height);

/**
 * @brief Set the maximum size for a widget
 * 
 * @param params  The layout parameters to modify
 * @param width   Maximum width in characters
 * @param height  Maximum height in characters
 */
void tui_layout_params_set_max_size(tui_layout_params_t* params, int width, int height);

/**
 * @brief Set whether a widget should expand to fill available space
 * 
 * @param params    The layout parameters to modify
 * @param expand_x  Whether to expand horizontally
 * @param expand_y  Whether to expand vertically
 */
void tui_layout_params_set_expand(tui_layout_params_t* params, bool expand_x, bool expand_y);

/**
 * @brief Set whether a widget should fill its allocated space
 * 
 * @param params   The layout parameters to modify
 * @param fill_x   Whether to fill horizontally
 * @param fill_y   Whether to fill vertically
 */
void tui_layout_params_set_fill(tui_layout_params_t* params, bool fill_x, bool fill_y);

/**
 * @brief Set the alignment of a widget within its allocated space
 * 
 * @param params   The layout parameters to modify
 * @param h_align  Horizontal alignment
 * @param v_align  Vertical alignment
 */
void tui_layout_params_set_alignment(tui_layout_params_t* params, 
                                   tui_horizontal_alignment_t h_align, 
                                   tui_vertical_alignment_t v_align);
/** @} */

/** @name Utility Functions */
/** @{ */

/**
 * @brief Align a rectangle within a container
 * 
 * @param container_size  Size of the container
 * @param element_size    Size of the element to align
 * @param h_align         Horizontal alignment
 * @param v_align         Vertical alignment
 * @return tui_rect_t     The aligned rectangle
 */
tui_rect_t tui_rect_align(tui_size_t container_size, tui_size_t element_size, 
                         tui_horizontal_alignment_t h_align, tui_vertical_alignment_t v_align);

/**
 * @brief Apply margins to a size
 * 
 * @param size     Original size
 * @param params   Layout parameters containing margins
 * @return tui_size_t    Size with margins applied
 */
tui_size_t tui_size_apply_margins(tui_size_t size, const tui_layout_params_t* params);

/**
 * @brief Apply padding to a size
 * 
 * @param size     Original size
 * @param params   Layout parameters containing padding
 * @return tui_size_t    Size with padding applied
 */
tui_size_t tui_size_apply_padding(tui_size_t size, const tui_layout_params_t* params);

/**
 * @brief Apply margins to a rectangle
 * 
 * @param rect     Original rectangle
 * @param params   Layout parameters containing margins
 * @return tui_rect_t    Rectangle with margins applied
 */
tui_rect_t tui_rect_apply_margins(tui_rect_t rect, const tui_layout_params_t* params);

/**
 * @brief Apply padding to a rectangle
 * 
 * @param rect     Original rectangle
 * @param params   Layout parameters containing padding
 * @return tui_rect_t    Rectangle with padding applied
 */
tui_rect_t tui_rect_apply_padding(tui_rect_t rect, const tui_layout_params_t* params);
/** @} */

/** @name Common Layout Functions */
/** @{ */

/**
 * @brief Layout all children of a container widget
 * 
 * @param container The container widget whose children should be laid out
 */
void tui_container_layout_children(tui_widget_t* container);

/**
 * @brief Measure the preferred size of a container's children
 * 
 * @param container The container widget whose children should be measured
 * @return tui_size_t     The preferred size needed to contain all children
 */
tui_size_t tui_container_measure_children(tui_widget_t* container);
/** @} */

/** @name Built-in Layout Implementations */
/** @{ */

/**
 * @brief Box layout implementation
 * 
 * @param container    The container widget to lay out
 * @param orientation  The orientation of the layout
 * @param spacing      Space between widgets in pixels
 */
void tui_layout_do_box(tui_widget_t* container, tui_orientation_t orientation, int spacing);

/**
 * @brief Grid layout implementation
 * 
 * @param container   The container widget to lay out
 * @param columns     Number of columns in the grid
 * @param h_spacing   Horizontal spacing between columns
 * @param v_spacing   Vertical spacing between rows
 */
void tui_layout_do_grid(tui_widget_t* container, int columns, int h_spacing, int v_spacing);

/**
 * @brief Border layout implementation
 * 
 * @param container  The container widget to lay out
 * @param spacing    Space between regions in pixels
 */
void tui_layout_do_border(tui_widget_t* container, int spacing);

/**
 * @brief Stack layout implementation
 * 
 * @param container  The container widget to lay out
 */
void tui_layout_do_stack(tui_widget_t* container);
/** @} */

#ifdef __cplusplus
}
#endif

#endif // TUI_LAYOUT_H
