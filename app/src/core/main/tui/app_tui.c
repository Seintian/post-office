#include "app_tui.h"
#include <postoffice/tui/tui.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h> // For KEY_UP, etc.
#include <tui/ui.h> // For dialogs

// Components
#include "components/topbar.h"
#include "components/sidebar.h"
#include "components/command_field.h"

// Screens
#include "screens/screen_dashboard.h"
#include "screens/screen_entities.h"
#include "screens/screen_template.h" 
#include "screens/screen_performance.h"

// Forward declaration
static void on_sidebar_select(tui_list_t* list, int index, void* data);

// Context
typedef struct {
    tui_widget_t* content_container;
    tui_widget_t* topbar;
    tui_list_t* sidebar_list;
    tui_window_t* main_window; // Wrap root in window concept for dialogs
} app_context_t;

static app_context_t g_ctx;

// Global event handler
static bool on_global_event(const tui_event_t* event, void* user_data) {
    (void)user_data;
    if (event->type == TUI_EVENT_KEY) {
        // Esc Handling
        if (event->data.key == 27) { // ESC
            // 1. Close active dialog if any
            if (g_ctx.main_window && g_ctx.main_window->active_dialog) {
                tui_dialog_close(g_ctx.main_window);
                return true;
            }

            // 2. Clear focus if any
            tui_widget_t* focused = tui_get_focused_widget();
            if (focused) {
                tui_set_focus(NULL);
                tui_widget_draw(tui_get_root()); // Redraw to clear cursor/focus styles
                return true;
            }
            
            // 3. Show modal if nothing else to do
            if (g_ctx.main_window) {
               tui_message_box_show(g_ctx.main_window, "Menu", "Feature coming soon...");
               return true;
            }
            return true;
        }

        // Sidebar Navigation (Up/Down)
        // Only if sidebar list is valid
        if (g_ctx.sidebar_list) {
            if (event->data.key == KEY_UP) {
                // Manually trigger list navigation
                int current = tui_list_get_selected_index(g_ctx.sidebar_list);
                if (current > 0) {
                    tui_list_set_selected_index(g_ctx.sidebar_list, current - 1);
                    on_sidebar_select(g_ctx.sidebar_list, current - 1, NULL);
                    tui_widget_draw((tui_widget_t*)g_ctx.sidebar_list);
                    return true; 
                }
            } else if (event->data.key == KEY_DOWN) {
                if (g_ctx.sidebar_list->selected_index < g_ctx.sidebar_list->item_count - 1) {
                    int next = g_ctx.sidebar_list->selected_index + 1;
                    tui_list_set_selected_index(g_ctx.sidebar_list, next);
                    on_sidebar_select(g_ctx.sidebar_list, next, NULL);
                    tui_widget_draw((tui_widget_t*)g_ctx.sidebar_list);
                    return true;
                }
            }
        }

        // Tab Navigation (Left/Right)
        if (g_ctx.content_container && g_ctx.content_container->type == TUI_WIDGET_PANEL) { // Actually content_container is panel
             tui_container_t* c = (tui_container_t*)g_ctx.content_container;
             if (c->first_child && c->first_child->type == TUI_WIDGET_TAB_CONTAINER) {
                 tui_tab_container_t* tabs = (tui_tab_container_t*)c->first_child;
                 if (event->data.key == KEY_LEFT) {
                     int curr = tabs->selected_tab;
                     if (curr > 0) {
                         tui_tab_container_set_selected_tab(tabs, curr - 1);
                         tui_widget_draw((tui_widget_t*)tabs);
                         return true;
                     }
                 } else if (event->data.key == KEY_RIGHT) {
                     int curr = tabs->selected_tab;
                     int count = tui_tab_container_get_tab_count(tabs);
                     if (curr < count - 1) {
                         tui_tab_container_set_selected_tab(tabs, curr + 1);
                         tui_widget_draw((tui_widget_t*)tabs);
                         return true;
                     }
                 }
             }
        }
    }
    return false;
}

// Callback for sidebar
static void on_sidebar_select(tui_list_t* list, int index, void* data) {
    (void)data;
    if (!g_ctx.content_container) return;

    // Clear current content
    tui_container_remove_all((tui_container_t*)g_ctx.content_container);

    tui_widget_t* new_content = NULL;
    const char* item_text = tui_list_get_item(list, index);
    
    if (item_text) {
        if (strcmp(item_text, "Director") == 0) {
            new_content = screen_dashboard_create();
        } else if (strcmp(item_text, "Ticket Issuer") == 0) {
            new_content = screen_entities_create("Ticket Issuer", 12);
        } else if (strcmp(item_text, "Users Manager") == 0) {
            new_content = screen_entities_create("Users Manager", 3);
        } else if (strcmp(item_text, "Worker") == 0) {
            new_content = screen_entities_create("Worker", 5);
        } else if (strcmp(item_text, "User") == 0) {
            new_content = screen_user_create();
        } else if (strcmp(item_text, "Performance") == 0) {
            new_content = screen_performance_create();
        }
    }

    if (new_content) {
        new_content->layout_params.expand_x = true;
        new_content->layout_params.expand_y = true;
        new_content->layout_params.fill_x = true;
        new_content->layout_params.fill_y = true;
        tui_container_add((tui_container_t*)g_ctx.content_container, new_content);
    }
    
    // Update status in topbar
    if (g_ctx.topbar) {
        char buf[64];
        snprintf(buf, sizeof(buf), "View: %s", item_text ? item_text : "Unknown");
        topbar_set_status(g_ctx.topbar, buf);
    }
}

void app_tui_run_simulation(void) {
    if (!tui_init()) return;

    tui_set_global_event_handler(on_global_event, NULL);

    tui_size_t screen = tui_get_screen_size();
    tui_rect_t bounds = {{0, 0}, screen};
    
    // Root container (Stack for Layers)
    tui_container_t* root = tui_container_create();
    tui_widget_set_bounds((tui_widget_t*)root, bounds);
    tui_container_set_layout(root, tui_layout_stack_create());
    tui_set_root((tui_widget_t*)root);

    // App Layer (Vertical Box)
    tui_container_t* app_layer = tui_container_create();
    // App layer should fill the screen
    app_layer->base.layout_params.expand_x = true;
    app_layer->base.layout_params.expand_y = true;
    app_layer->base.layout_params.fill_x = true;
    app_layer->base.layout_params.fill_y = true;
    
    tui_container_set_layout(app_layer, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0));
    tui_container_add(root, (tui_widget_t*)app_layer);

    // Setup Window context for dialogs (wraps root stack)
    g_ctx.main_window = calloc(1, sizeof(tui_window_t));
    g_ctx.main_window->root = (tui_widget_t*)root;
    g_ctx.main_window->running = true;

    // 1. Topbar (Added to App Layer)
    tui_widget_t* topbar = topbar_create();
    topbar->layout_params.fill_x = true;
    topbar->layout_params.min_height = 3;
    tui_container_add(app_layer, topbar);
    g_ctx.topbar = topbar;

    // 2. Middle (Sidebar + Content)
    tui_container_t* middle = tui_container_create();
    middle->base.layout_params.weight_y = 1.0f;
    middle->base.layout_params.fill_x = true;
    middle->base.layout_params.expand_y = true;
    tui_container_set_layout(middle, tui_layout_box_create(TUI_ORIENTATION_HORIZONTAL, 0));
    tui_container_add(app_layer, (tui_widget_t*)middle);

    // Sidebar
    tui_widget_t* sidebar = sidebar_create(on_sidebar_select, NULL, &g_ctx.sidebar_list);
    sidebar->layout_params.min_width = 25;
    sidebar->layout_params.expand_y = true;
    tui_container_add(middle, sidebar);

    // Content
    tui_container_t* content = tui_container_create();
    content->base.layout_params.weight_x = 1.0f; 
    content->base.layout_params.expand_y = true; 
    content->base.layout_params.fill_x = true;
    tui_container_set_layout(content, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0));
    // Margin to separate from sidebar
    tui_layout_params_set_margin(&content->base.layout_params, 1, 0, 0, 0);
    tui_container_add(middle, (tui_widget_t*)content);
    g_ctx.content_container = (tui_widget_t*)content;

    // 3. Command Field
    tui_widget_t* cmd = command_field_create();
    cmd->layout_params.fill_x = true;
    cmd->layout_params.min_height = 3; 
    // Margin bottom removed as field invalid
    tui_container_add(app_layer, cmd);

    // Initial Screen
    tui_widget_t* start_screen = screen_dashboard_create();
    start_screen->layout_params.expand_x = true;
    start_screen->layout_params.expand_y = true;
    start_screen->layout_params.fill_x = true;
    start_screen->layout_params.fill_y = true;
    tui_container_add((tui_container_t*)g_ctx.content_container, start_screen);

    tui_render();
    tui_run();
    tui_cleanup();
}

void app_tui_run_demo(void) {
    app_tui_run_simulation();
}
