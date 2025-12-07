/**
 * @file ui.c
 * @brief TUI UI Framework Implementation
 */

#include "ui.h"
#include <tui/tui.h>
#include "widgets.h"
#include <stdlib.h>
#include <string.h>

// --- Window ---

tui_window_t* tui_window_create(const char* title) {
    tui_window_t* win = malloc(sizeof(tui_window_t));
    memset(win, 0, sizeof(tui_window_t));
    
    // Create root panel for window
    tui_size_t screen_size = tui_get_screen_size();
    tui_rect_t bounds = {{0, 0}, screen_size};
    
    // The window itself is typically managed by TUI root, but `tui_window_t` struct implies it HOLDS a root.
    // In many TUI libs, Window IS a widget. Here it wraps widgets.
    
    win->root = (tui_widget_t*)tui_panel_create(bounds, title);
    // Usually main window has a menu bar, status bar, and content.
    // We'll use a vertical box layout for the root panel if we can, or manual.
    // For simplicity, let's just say root is the main container.
    
    // tui_set_root(win->root); // The app might set this active later
    return win;
}

void tui_window_destroy(tui_window_t* window) {
    if (!window) return;
    tui_widget_destroy(window->root);
    free(window);
}

void tui_window_set_content(tui_window_t* window, tui_widget_t* content) {
    if (!window) return;
    // For now, just add as child to root panel, but we might want to replace existing content
    // Assuming root is a container/panel
    tui_container_add((tui_container_t*)window->root, content);
}

void tui_window_show(tui_window_t* window) {
    if (!window) return;
    tui_set_root(window->root);
}

void tui_window_run(tui_window_t* window) {
    if (!window) return;
    window->running = true;
    tui_window_show(window);
    tui_run();
}

// --- Screen ---

struct tui_screen_t {
    tui_widget_t* root;
    char* title;
};

tui_screen_t* tui_screen_create(tui_widget_t* content) {
    tui_screen_t* s = malloc(sizeof(tui_screen_t));
    s->root = content;
    s->title = NULL;
    return s;
}

void tui_screen_destroy(tui_screen_t* screen) {
    if (screen) {
        // destroy root? Owner vs Observer is ambiguous, usually screens own their widgets
        tui_widget_destroy(screen->root);
        free(screen->title);
        free(screen);
    }
}

static tui_screen_t* g_active_screen = NULL;

void tui_screen_show(tui_screen_t* screen) {
    if (!screen) return;
    g_active_screen = screen;
    tui_set_root(screen->root);
}

// --- Dialogs ---
void tui_dialog_show(tui_window_t* window, tui_widget_t* dialog) {
    if (!window || !dialog) return;
    // Simple implementation: Replace root or add to root
    // For now, let's just add it to root for the demo
    if (window->root) {
        tui_container_add((tui_container_t*)window->root, dialog);
        tui_set_focus(dialog);
       window->active_dialog = dialog;
    }
}

void tui_dialog_close(tui_window_t* window) {
    if (!window || !window->active_dialog) return;
    // Remove from root
    if (window->root) {
        tui_container_remove((tui_container_t*)window->root, window->active_dialog);
        tui_widget_destroy(window->active_dialog); // Assuming dialog is owned by window when shown
        window->active_dialog = NULL;
    }
}

// Callback for message box OK button
static void on_message_box_ok(tui_button_t* btn, void* data) {
    (void)btn;
    tui_window_t* parent = (tui_window_t*)data;
    tui_dialog_close(parent);
}

// Simple message box using high-level dialog structure
void tui_message_box_show(tui_window_t* parent, const char* title, const char* message) {
    if (!parent) return;
    
    tui_size_t screen = tui_get_screen_size();
    tui_rect_t bounds = {{(int16_t)(screen.width/4), (int16_t)(screen.height/3)}, {(int16_t)(screen.width/2), (int16_t)(screen.height/3)}};
    
    tui_panel_t* dlg = tui_panel_create(bounds, title);
    
    // Text
    tui_label_t* lbl = tui_label_create(message, (tui_point_t){(int16_t)(bounds.position.x+2), (int16_t)(bounds.position.y+2)});
    tui_container_add((tui_container_t*)dlg, (tui_widget_t*)lbl);
    
    // OK Button
    tui_rect_t btn_bounds = {{(int16_t)(bounds.position.x + bounds.size.width/2 - 5), (int16_t)(bounds.position.y + bounds.size.height - 2)}, {10, 1}};
    tui_button_t* btn = tui_button_create("OK", btn_bounds);
    tui_button_set_click_callback(btn, on_message_box_ok, parent);
    
    tui_container_add((tui_container_t*)dlg, (tui_widget_t*)btn);
    
    tui_dialog_show(parent, (tui_widget_t*)dlg);
}

// Implement other stubs to satisfy linker
void tui_theme_init(tui_theme_t* theme) { (void)theme; }
void tui_window_set_theme(tui_window_t* window, const tui_theme_t* theme) { (void)window; (void)theme; }
const tui_theme_t* tui_window_get_theme(const tui_window_t* window) { (void)window; return NULL; }
char* tui_input_box_show(tui_window_t* parent, const char* title, const char* message, const char* default_text) { 
    (void)parent; (void)title; (void)message; (void)default_text;
    return NULL; 
}
char* tui_file_dialog_open(tui_window_t* parent, const char* title, const char* filter) { 
    (void)parent; (void)title; (void)filter;
    return NULL; 
}
