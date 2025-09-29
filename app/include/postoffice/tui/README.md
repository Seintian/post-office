# Post Office TUI Library

A lightweight, widget-based terminal user interface library for the Post Office project. This library provides a high-level C API for creating rich terminal applications with a focus on simplicity and performance.

## Table of Contents

- [Features](#features)
- [Getting Started](#getting-started)
  - [Basic Usage](#basic-usage)
- [Architecture](#architecture)
- [Building Blocks](#building-blocks)
  - [Widgets Overview](#widgets-overview)
  - [Layout System Overview](#layout-system-overview)
  - [Event Handling Overview](#event-handling-overview)
- [Widget Reference](#widget-reference)
  - [Basic Widgets](#basic-widgets)
  - [Container Widgets](#container-widgets)
  - [Input Widgets](#input-widgets)
  - [Dialog Windows](#dialog-windows)
- [Layout System](#layout-system)
  - [Layout Parameters](#layout-parameters)
  - [Example: Setting Layout Parameters](#example-setting-layout-parameters)
  - [Layout Managers](#layout-managers)
    - [Box Layout](#box-layout)
    - [Grid Layout](#grid-layout)
  - [Custom Layouts](#custom-layouts)
- [Event System](#event-system)
  - [Event Types](#event-types)
  - [Event Handling Example](#event-handling-example)
  - [Button Click](#button-click)
  - [Key Press](#key-press)
  - [Window Close](#window-close)
- [Theming](#theming)
  - [Theme Properties](#theme-properties)
- [Best Practices](#best-practices)
- [Code Examples](#code-examples)
  - [Creating a Form](#creating-a-form)
  - [Creating a List with Selection](#creating-a-list-with-selection)
  - [Layout System in Practice](#layout-system-in-practice)
- [Advanced Event Handling](#advanced-event-handling)
- [Build and Installation](#build-and-installation)
  - [Prerequisites](#prerequisites)
  - [Building with CMake](#building-with-cmake)
  - [Including in Your Project](#including-in-your-project)

## Features

- **Widget-based architecture** - Create complex UIs by composing simple widgets
- **Flexible layout system** - Supports flexible and grid-based layouts with constraints
- **Rich widget set** - Buttons, labels, panels, progress bars, and more
- **Event-driven** - Clean callback-based event handling
- **Lightweight** - Minimal dependencies, designed for embedded systems

## Getting Started

### Basic Usage

```c
#include <postoffice/tui/tui.h>
#include <postoffice/tui/widgets.h>
#include <postoffice/tui/layout.h>

// Button click callback
void on_button_click(tui_widget_t* widget, void* user_data) {
    static int counter = 0;
    counter++;

    // Update the label text
    tui_label_t* label = (tui_label_t*)user_data;
    char buf[64];
    snprintf(buf, sizeof(buf), "Button clicked %d times", counter);
    tui_label_set_text(label, buf);
}

int main() {
    // Initialize the TUI system
    tui_init();

    // Create a window
    tui_window_t* window = tui_window_create("TUI Example");

    // Create a panel for the main content
    tui_panel_t* panel = tui_panel_create(tui_rect_make(1, 1, 58, 18), "Main Panel");
    tui_window_set_content(window, (tui_widget_t*)panel);

    // Create a button
    tui_button_t* button = tui_button_create("Click Me!", tui_point_make(10, 5));

    // Create a label
    tui_label_t* label = tui_label_create("Button not clicked yet", tui_point_make(10, 2));

    // Set button click callback
    tui_button_set_click_callback(button, on_button_click, label);

    // Add widgets to the panel
    tui_container_add((tui_container_t*)panel, (tui_widget_t*)button);
    tui_container_add((tui_container_t*)panel, (tui_widget_t*)label);

    // Set up a simple layout
    tui_layout_manager_t* layout = tui_create_box_layout(TUI_ORIENTATION_VERTICAL, 1);
    tui_container_set_layout((tui_container_t*)panel, layout);

    // Show the window
    tui_window_show(window);

    // Main event loop
    bool running = true;
    tui_event_t event;
    while (running && tui_poll_event(&event)) {
        if (event.type == TUI_EVENT_QUIT) {
            running = false;
        }
        tui_window_handle_event(window, &event);
        tui_render();
    }

    // Cleanup
    tui_window_destroy(window);
    tui_cleanup();
    return 0;
}
```

## Architecture

The TUI library is organized into several key components:

- **Core** (`tui.h`): Core functionality, initialization, and main event loop
- **Widgets** (`widgets.h`): UI components like buttons, labels, and containers
- **Layout** (`layout.h`): Layout managers and constraints system
- **Types** (`types.h`): Common types, constants, and enumerations
- **UI** (`ui.h`): High-level UI components and window management

## Building Blocks

### Widgets Overview

All widgets inherit from the base `tui_widget_t` structure which provides common properties:

- Position and size
- Visibility and enabled state
- Event handling
- Parent/child relationships

### Layout System Overview

The layout system uses a constraint-based approach where each widget can specify:

- Minimum and maximum dimensions
- Margins and padding
- Alignment within available space
- Weight for flexible sizing

### Event Handling Overview

Events in the TUI system are processed through a hierarchical system:

1. Events are first sent to the focused widget
2. If not handled, they bubble up to parent widgets
3. Finally, they're processed by the window and application

## Widget Reference

The TUI library provides the following widgets:

### Basic Widgets

- **Widget** (`tui_widget_t`) - Base class for all UI elements
- **Container** (`tui_container_t`) - Manages child widgets and layout
- **Label** (`tui_label_t`) - Displays static text with alignment options
- **Button** (`tui_button_t`) - Clickable button with text and callback
- **Panel** (`tui_panel_t`) - Container with border and optional title
- **Progress Bar** (`tui_progress_bar_t`) - Visual progress indicator

### Container Widgets

- **Panel** (`tui_panel_t`) - Basic container with border and title
- **Tab Container** (`tui_tab_container_t`) - Tabbed interface for multiple pages
- **Menu Bar** (`tui_menu_bar_t`) - Horizontal menu with dropdowns
- **Status Bar** (`tui_status_bar_t`) - Displays status messages and information
- **Scroll View** (`tui_scroll_view_t`) - Scrollable container for large content

### Input Widgets

- **Input Field** (`tui_input_field_t`) - Single-line text input
- **List** (`tui_list_t`) - Scrollable list of selectable items
- **Checkbox** (`tui_checkbox_t`) - Toggleable option
- **Radio Button** (`tui_radio_button_t`) - Single selection from a group
- **Combo Box** (`tui_combo_box_t`) - Dropdown selection list
- **Slider** (`tui_slider_t`) - Interactive value selector

### Dialog Windows

- **Message Box** - Simple dialog with message and buttons
- **File Dialog** - File open/save dialog
- **Input Dialog** - Prompt for user input

```c
// Example: Show a message dialog
tui_dialog_show_message(window, "Information", "Operation completed successfully!");

// Example: Show a confirmation dialog
if (tui_dialog_show_confirm(window, "Confirm", "Are you sure?")) {
    // User clicked Yes
} else {
    // User clicked No or closed the dialog
}
```

## Layout System

This section details the layout system that uses constraints to position and size widgets. Each widget can specify:

### Layout Parameters

```c
typedef struct {
    int min_width;        // Minimum width
    int max_width;        // Maximum width (0 for no limit)
    int min_height;       // Minimum height
    int max_height;       // Maximum height (0 for no limit)
    float weight_x;       // Horizontal weight for flexible sizing
    float weight_y;       // Vertical weight for flexible sizing
    struct {
        int left;         // Left margin
        int top;          // Top margin
        int right;        // Right margin
        int bottom;       // Bottom margin
    } margin;            // Outer spacing
    struct {
        int left;         // Left padding
        int top;          // Top padding
        int right;        // Right padding
        int bottom;       // Bottom padding
    } padding;           // Inner spacing
    tui_horizontal_alignment_t h_align;  // Horizontal alignment
    tui_vertical_alignment_t v_align;    // Vertical alignment
    bool expand_x;        // Whether to expand horizontally
    bool expand_y;        // Whether to expand vertically
    bool fill_x;          // Whether to fill available horizontal space
    bool fill_y;          // Whether to fill available vertical space
} tui_layout_params_t;
```

### Example: Setting Layout Parameters

```c
// Create a button
tui_button_t* button = tui_button_create("Click Me", tui_point_make(0, 0));

// Set layout parameters
tui_layout_params_t params = {
    .min_width = 10,
    .min_height = 3,
    .margin = {.left = 1, .top = 1, .right = 1, .bottom = 1},
    .h_align = TUI_ALIGN_CENTER,
    .v_align = TUI_ALIGN_MIDDLE,
    .expand_x = true,
    .fill_x = true
};

tui_widget_set_layout_params((tui_widget_t*)button, &params);
```

### Layout Managers

The library provides several layout managers:

#### Box Layout

Arranges widgets in a single row or column:

```c
// Create a vertical box layout with 10px spacing
tui_layout_manager_t* layout = tui_create_box_layout(TUI_ORIENTATION_VERTICAL, 10);
tui_container_set_layout(container, layout);

// Add widgets to the container
tui_container_add(container, (tui_widget_t*)label1);
tui_container_add(container, (tui_widget_t*)input1);
tui_container_add(container, (tui_widget_t*)button1);
```

#### Grid Layout

Arranges widgets in a grid with specified rows and columns:

```c
// Create a 2x2 grid with 5px horizontal and vertical spacing
tui_layout_manager_t* grid = tui_create_grid_layout(2, 2, 5, 5);
tui_container_set_layout(container, grid);

// Add widgets to the grid
tui_container_add(container, (tui_widget_t*)widget1);
tui_container_add(container, (tui_widget_t*)widget2);
tui_container_add(container, (tui_widget_t*)widget3);
tui_container_add(container, (tui_widget_t*)widget4);
```

### Custom Layouts

You can create custom layout managers by implementing the `tui_layout_manager_t` interface:

```c
typedef struct {
    void (*layout)(tui_container_t* container);
    void (*measure)(tui_container_t* container, tui_size_t* size);
    void* user_data;
} tui_layout_manager_t;
```

## Event System

### Event Types

```c
typedef enum {
    TUI_EVENT_NONE,
    TUI_EVENT_QUIT,
    TUI_EVENT_KEY_PRESS,
    TUI_EVENT_KEY_RELEASE,
    TUI_EVENT_MOUSE_MOVE,
    TUI_EVENT_MOUSE_PRESS,
    TUI_EVENT_MOUSE_RELEASE,
    TUI_EVENT_WIDGET,
    // ... other event types
} tui_event_type_t;
```

### Event Structure

```c
typedef struct {
    tui_event_type_t type;
    union {
        struct {
            int key;
            int mods;
        } key;
        struct {
            tui_point_t pos;
            int button;
            int mods;
        } mouse;
        // ... other event data
    } data;
} tui_event_t;
```

### Event Handling Example

```c
// Handle button click
void on_button_click(tui_widget_t* widget, void* user_data) {
    tui_label_t* label = (tui_label_t*)user_data;
    tui_label_set_text(label, "Button clicked!");
}

// Set up button click handler
tui_button_set_click_callback(button, on_button_click, label);

// Main event loop
tui_event_t event;
while (tui_poll_event(&event)) {
    if (event.type == TUI_EVENT_QUIT) {
        break;
    }

    // Handle window events
    tui_window_handle_event(window, &event);

    // Render the UI
    tui_render();
}
```

Event handling is done through callback functions. Here's how to handle common events:

### Button Click

```c
void on_button_click(Button* button, void* user_data) {
    // Handle button click
}

Button* btn = tui_button_create("Click Me!", (Point){0, 0}, (Size){10, 3});
tui_button_set_on_click(btn, on_button_click, NULL);
```

### Key Press

```c
void on_key_pressed(Widget* widget, const Event* event, void* user_data) {
    if (event->type == EVENT_KEY) {
        if (event->data.key == 'q') {
            tui_quit();
        }
    }
}

tui_widget_set_on_key_pressed(widget, on_key_pressed, NULL);
```

### Window Close

```c
bool on_window_close(Window* window, void* user_data) {
    // Return true to allow closing, false to prevent
    return true;
}

tui_window_set_on_close(window, on_window_close, NULL);
```

## Theming

The TUI library provides a flexible theming system to customize the appearance of your application.

### Theme Properties

Here are the available theme properties you can customize:

- Window background/foreground
- Button styles (normal, hover, pressed, disabled)
- Text colors
- Border styles
- Focus indicators

## Best Practices

1. **Memory Management**
   - Always pair [`tui_init()`](tui.h#L28) with [`tui_cleanup()`](tui.h#L36)
   - Destroy windows and widgets when no longer needed
   - Use [`tui_widget_destroy()`](widgets.h#L252) for proper cleanup

2. **Event Handling**
   - Keep event handlers short and fast
   - Use user_data to pass context to callbacks
   - Handle errors gracefully

3. **Thread Safety**
   - TUI functions are not thread-safe
   - Perform UI updates from the main thread only
   - Use message queues for inter-thread communication

4. **Performance**
   - Batch UI updates when possible
   - Only redraw what's necessary
   - Use [`tui_widget_set_visible()`](widgets.h#L219) to hide/show widgets instead of recreating them

## Code Examples

### Creating a Form

```c
// Create a container with vertical layout
Container* form = tui_container_create();
tui_widget_set_layout((Widget*)form, tui_create_box_layout(ORIENTATION_VERTICAL, 1));

// Add form fields
Label* name_label = tui_label_create("Name:", (Point){0, 0});
InputField* name_input = tui_input_field_create((Point){10, 0}, 20);
tui_container_add(form, (Widget*)name_label);
tui_container_add(form, (Widget*)name_input);

// Add buttons
Container* button_row = tui_container_create();
tui_widget_set_layout((Widget*)button_row, tui_create_box_layout(ORIENTATION_HORIZONTAL, 2));

Button* ok_btn = tui_button_create("OK", (Point){0, 0}, (Size){8, 1});
Button* cancel_btn = tui_button_create("Cancel", (Point){0, 0}, (Size){8, 1});

tui_container_add(button_row, (Widget*)ok_btn);
tui_container_add(button_row, (Widget*)cancel_btn);
tui_container_add(form, (Widget*)button_row);
```

### Creating a List with Selection

```c
void on_item_selected(List* list, int selected, void* user_data) {
    const char* item = tui_list_get_item(list, selected);
    printf("Selected: %s\n", item);
}

// Create a list with bounds
List* list = tui_list_create((Rect){0, 0, 30, 10});

// Add items to the list
const char* items[] = {"Item 1", "Item 2", "Item 3", "Item 4", "Item 5"};
for (int i = 0; i < 5; i++) {
    tui_list_add_item(list, items[i]);
}

// Set the selection callback
tui_list_set_on_select(list, on_item_selected, NULL);

// Optionally, set initial selection
tui_list_set_selected(list, 0);
```

## Layout System in Practice

Here's how you can use the layout system to organize your widgets:

- **BoxLayout** - Arranges widgets in a horizontal or vertical line
- **GridLayout** - Arranges widgets in a grid
- **BorderLayout** - Divides space into five regions: north, south, east, west, and center
- **StackLayout** - Stacks widgets on top of each other

## Advanced Event Handling

For more complex scenarios, you can handle user input with these event callbacks:

```c
void on_key_pressed(Widget* widget, const Event* event, void* user_data) {
    if (event->type == EVENT_KEY) {
        // Handle key press
        if (event->data.key == 'q') {
            tui_window_exit(tui_get_active_window());
        }
    }
}
```

## Build and Installation

### Prerequisites

- C99 or later compiler
- ncursesw library (with wide character support)
- CMake 3.10+

### Building with CMake

1. Create a build directory and navigate into it:

    ```bash
    mkdir -p build && cd build
    ```

2. Run CMake to configure the project:

    ```bash
    cmake .. -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DTUI_BUILD_EXAMPLES=ON \
        -DTUI_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local
    ```

3. Build the project:

    ```bash
    make -j$(nproc)
    ```

### Including in Your Project

Add these lines to your project's CMakeLists.txt:

```cmake
# Find the TUI package
find_package(TUI REQUIRED)

# Link against the TUI library
target_link_libraries(your_target_name PRIVATE TUI::TUI)
```
