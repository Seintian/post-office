/**
 * @file ui.c
 * @brief TUI UI Framework Implementation
 */

#include "ui.h"
#include <tui/tui.h>
#include "widgets.h"
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

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
        tui_widget_destroy(window->active_dialog); 
        window->active_dialog = NULL;
        
        // Redraw everything to clear the "ghost" of the dialog
        clear();
        tui_widget_draw(window->root);
        refresh();
    }
}

// Callback for message box OK button
static void on_message_box_ok(tui_button_t* btn, void* data) {
    (void)btn;
    tui_window_t* parent = (tui_window_t*)data;
    tui_dialog_close(parent);
}

// Custom draw for dialog to fill background
static void tui_dialog_draw(tui_widget_t* widget) {
    tui_panel_t* p = (tui_panel_t*)widget;
    tui_rect_t b = widget->bounds;
    
    // Clear background
    for(int y = b.position.y; y < b.position.y + b.size.height; y++) {
        mvhline(y, b.position.x, ' ', b.size.width);
    }
    
    // Draw standard panel (border/title/children)
    // We can't easily call tui_panel_draw because it's static in widgets.c
    // But we can replicate the border drawing or rely on tui_panel_draw if we could access it.
    // Since we can't access it, we'll reimplement basic border here.
    
    if (p->show_border) {
        // Corners
        mvaddch(b.position.y, b.position.x, ACS_ULCORNER);
        mvaddch(b.position.y, b.position.x + b.size.width - 1, ACS_URCORNER);
        mvaddch(b.position.y + b.size.height - 1, b.position.x, ACS_LLCORNER);
        mvaddch(b.position.y + b.size.height - 1, b.position.x + b.size.width - 1, ACS_LRCORNER);
        
        // Sides
        mvhline(b.position.y, b.position.x + 1, ACS_HLINE, b.size.width - 2);
        mvhline(b.position.y + b.size.height - 1, b.position.x + 1, ACS_HLINE, b.size.width - 2);
        mvvline(b.position.y + 1, b.position.x, ACS_VLINE, b.size.height - 2);
        mvvline(b.position.y + 1, b.position.x + b.size.width - 1, ACS_VLINE, b.size.height - 2);

        if (p->title) {
            mvprintw(b.position.y, b.position.x + 2, " %s ", p->title);
        }
    }
    
    // Draw children
    // We can cast to container and iterate, but we need tui_container_draw which is static in widgets.c?
    // tui_container_draw IS static in widgets.c.
    // However, tui_widget_draw on children works.
    tui_container_t* c = (tui_container_t*)widget;
    tui_widget_t* child = c->first_child;
    while (child) {
        tui_widget_draw(child);
        child = child->next;
    }
}

// Simple message box using high-level dialog structure
void tui_message_box_show(tui_window_t* parent, const char* title, const char* message) {
    if (!parent) return;
    
    tui_size_t screen = tui_get_screen_size();
    
    // We create the panel with explicit bounds, but since it will be in a StackLayout,
    // we MUST set layout params to control its size and position.
    // The bounds passed to create() are mostly initial.
    tui_rect_t bounds = {{(int16_t)(screen.width/4), (int16_t)(screen.height/3)}, {(int16_t)(screen.width/2), (int16_t)(screen.height/3)}};
    
    tui_panel_t* dlg = tui_panel_create(bounds, title);
    // Override draw
    dlg->base.base.draw = tui_dialog_draw;

    // Stack Layout Params for the Dialog itself
    tui_layout_params_set_alignment(&dlg->base.base.layout_params, TUI_ALIGN_CENTER, TUI_ALIGN_MIDDLE);
    // Fix size to 1/2 width, 1/3 height
    dlg->base.base.layout_params.min_width = screen.width / 2;
    dlg->base.base.layout_params.max_width = screen.width / 2;
    dlg->base.base.layout_params.min_height = screen.height / 3;
    dlg->base.base.layout_params.max_height = screen.height / 3;

    // Use a Vertical Box layout for the Dialog's content
    tui_container_set_layout((tui_container_t*)dlg, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1)); // 1 line spacing
    // Padding inside the dialog
    tui_layout_params_set_padding(&dlg->base.base.layout_params, 2, 2, 2, 1);
    
    // Text
    // Note: Position passed to create is ignored by Box Layout
    tui_label_t* lbl = tui_label_create(message, (tui_point_t){0,0});
    // Center the label horizontally
    tui_layout_params_set_alignment(&lbl->base.layout_params, TUI_ALIGN_CENTER, TUI_ALIGN_MIDDLE);
    // Let text expand to fill width to allow centering
    lbl->base.layout_params.fill_x = true;
    lbl->base.layout_params.weight_y = 1.0f; // Push button down
    
    tui_container_add((tui_container_t*)dlg, (tui_widget_t*)lbl);
    
    // OK Button
    tui_rect_t btn_bounds = {{0,0}, {10, 1}};
    tui_button_t* btn = tui_button_create("OK", btn_bounds);
    tui_button_set_click_callback(btn, on_message_box_ok, parent);
    
    // Center button
    tui_layout_params_set_alignment(&btn->base.layout_params, TUI_ALIGN_CENTER, TUI_ALIGN_BOTTOM);
    // Fixed size
    btn->base.layout_params.min_width = 10;
    btn->base.layout_params.min_height = 1;
    
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
