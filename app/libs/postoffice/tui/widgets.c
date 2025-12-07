/**
 * @file widgets.c
 * @brief TUI Widgets Implementation
 */

#define _GNU_SOURCE 1 // for asprintf if needed

#include "widgets.h"
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include "tui/tui.h" // For context if exposed, else extern

// --- Base Widget ---

void tui_widget_init(tui_widget_t* widget, tui_widget_type_t type) {
    if (!widget) return;
    memset(widget, 0, sizeof(tui_widget_t));
    widget->type = type;
    widget->visible = true;
    widget->enabled = true;
    tui_layout_params_init(&widget->layout_params);
}

void tui_widget_destroy(tui_widget_t* widget) {
    if (!widget) return;
    if (tui_get_focused_widget() == widget) {
        tui_set_focus(NULL);
    }
    if (widget->free) widget->free(widget);
    // Base cannot free 'widget' itself if it was stack allocated or embedded
    // But typically create() uses malloc. 
    // The specific destroy functions below will handle the free(widget) part.
}

void tui_widget_draw(tui_widget_t* widget) {
    if (!widget || !widget->visible) return;
    if (widget->draw) widget->draw(widget);
}

bool tui_widget_handle_event(tui_widget_t* widget, const tui_event_t* event) {
    if (!widget || !widget->enabled || !widget->visible) return false;
    if (widget->handle_event) return widget->handle_event(widget, event);
    return false;
}

bool tui_widget_contains_point(const tui_widget_t* widget, tui_point_t point) {
    if (!widget) return false;
    return point.x >= widget->bounds.position.x &&
           point.x < widget->bounds.position.x + widget->bounds.size.width &&
           point.y >= widget->bounds.position.y &&
           point.y < widget->bounds.position.y + widget->bounds.size.height;
}

tui_widget_t* tui_widget_find_at(tui_widget_t* widget, tui_point_t point) {
    if (!widget || !widget->visible || !tui_widget_contains_point(widget, point)) return NULL;
    
    // If container, check children in reverse order (top-most first)
    if (widget->type == TUI_WIDGET_PANEL || widget->type == TUI_WIDGET_WINDOW) { // Loose type check for containers
         tui_container_t* c = (tui_container_t*)widget;
         // Since we have a singly linked list, we can't easily iterate reverse.
         // We'll iterate forward and keep the last match.
         tui_widget_t* child = c->first_child;
         tui_widget_t* found = NULL;
         while (child) {
             tui_widget_t* hit = tui_widget_find_at(child, point);
             if (hit) found = hit;
             child = child->next;
         }
         if (found) return found;
    }
    
    return widget; // Leaf or no child hit
}

void tui_widget_set_position(tui_widget_t* widget, tui_point_t pos) {
    if(widget) widget->bounds.position = pos;
}
void tui_widget_set_size(tui_widget_t* widget, tui_size_t size) {
    if(widget) widget->bounds.size = size;
}
void tui_widget_set_bounds(tui_widget_t* widget, tui_rect_t bounds) {
    if(widget) widget->bounds = bounds;
}

// --- Container ---

static void tui_container_draw(tui_widget_t* widget) {
    tui_container_t* container = (tui_container_t*)widget;
    // Draw children
    tui_widget_t* child = container->first_child;
    while (child) {
        tui_widget_draw(child);
        child = child->next;
    }
}

static bool tui_container_handle_event(tui_widget_t* widget, const tui_event_t* event) {
    (void)widget; (void)event;
    // Container usually forwards events to children if not handled?
    // In tui.c we handle bubbling from focused/root.
    // If we are here, it might be a direct event to container.
    return false;
}

static void tui_container_free_impl(tui_widget_t* widget) {
    tui_container_t* container = (tui_container_t*)widget;
    // Free children
    tui_container_remove_all(container);
    if(container->layout && container->layout->free) container->layout->free(container->layout);
    free(container);
}

tui_container_t* tui_container_create(void) {
    tui_container_t* c = malloc(sizeof(tui_container_t));
    tui_widget_init(&c->base, TUI_WIDGET_PANEL); // Use PANEL type for generic container for now
    c->base.draw = tui_container_draw;
    c->base.handle_event = tui_container_handle_event;
    c->base.free = tui_container_free_impl;
    c->first_child = NULL;
    c->last_child = NULL;
    c->child_count = 0;
    c->layout = NULL;
    c->auto_delete_children = true;
    return c;
}

void tui_container_add(tui_container_t* container, tui_widget_t* child) {
    if (!container || !child) return;
    
    child->parent = (tui_widget_t*)container;
    child->next = NULL;
    
    if (container->last_child) {
        container->last_child->next = child;
        container->last_child = child;
    } else {
        container->first_child = child;
        container->last_child = child;
    }
    container->child_count++;
    
    // Trigger layout update if auto-layout is active?
    if (container->layout) {
        container->layout->layout(container->layout, (tui_widget_t*)container);
    }
}

void tui_container_remove(tui_container_t* container, tui_widget_t* child) {
    if (!container || !child) return;
    
    tui_widget_t* prev = NULL;
    tui_widget_t* curr = container->first_child;
    
    while (curr) {
        if (curr == child) {
            if (prev) {
                prev->next = curr->next;
            } else {
                container->first_child = curr->next;
            }
            if (curr == container->last_child) {
                container->last_child = prev;
            }
            container->child_count--;
            child->parent = NULL;
            child->next = NULL; // Detach
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void tui_container_remove_all(tui_container_t* container) {
    if (!container) return;
    tui_widget_t* curr = container->first_child;
    while (curr) {
        tui_widget_t* next = curr->next;
        if (container->auto_delete_children) {
            tui_widget_destroy(curr); // This calls free()
        }
        curr = next;
    }
    container->first_child = NULL;
    container->last_child = NULL;
    container->child_count = 0;
}

// --- Button ---

static void tui_button_draw(tui_widget_t* widget) {
    tui_button_t* btn = (tui_button_t*)widget;
    
    // Draw background
    int attrs = 0;
    if (widget->has_focus) attrs |= (int)A_REVERSE;
    if (btn->is_pressed) attrs |= (int)A_BOLD;

    // Use ncurses box or fill
    // Simple ASCII button [ Text ]
    
    tui_rect_t b = widget->bounds;
    // Draw manually
    // move(b.position.y, b.position.x); // etc. but ncurses uses global `move`.
    
    // Helper to draw
    attron(attrs);
    mvprintw(b.position.y, b.position.x, "[ %s ]", btn->text ? btn->text : "");
    attroff(attrs);
}

static bool tui_button_handle_event(tui_widget_t* widget, const tui_event_t* event) {
    tui_button_t* btn = (tui_button_t*)widget;
    
    if (event->type == TUI_EVENT_MOUSE) {
        mmask_t bstate = event->data.mouse.button;
        
        // Left Click Press
        if (bstate & BUTTON1_PRESSED) {
            btn->is_pressed = true;
            tui_widget_draw(widget);
            refresh();
            return true;
        }
        
        // Left Click Release (Action)
        if ((bstate & (BUTTON1_RELEASED | BUTTON1_CLICKED)) && btn->is_pressed) {
            btn->is_pressed = false;
            tui_widget_draw(widget); 
            refresh();
            if (btn->callback) btn->callback(btn, widget->user_data);
            return true;
        }
        
        // Handle case where we released outside or something else cleared press?
        // Actually if we just get a non-press event and we were pressed, maybe reset visual state but don't fire?
        // For now, strict Release check is better for "user behavior".
        
        // Right Click
        if (bstate & (BUTTON3_CLICKED | BUTTON3_RELEASED | BUTTON3_PRESSED)) { 
            // Usually trigger on release/click for context menu behavior
            if (bstate & (BUTTON3_CLICKED | BUTTON3_RELEASED)) {
                if (btn->on_right_click) btn->on_right_click(btn, widget->user_data);
            }
            return true;
        }
        
        // Scroll
        // NCURSES usually maps wheel to Button 4 and 5
        #ifndef BUTTON4_PRESSED
        #define BUTTON4_PRESSED (1 << 21) // common mapping if not defined
        #define BUTTON5_PRESSED (1 << 22)
        #endif
        
        if (bstate & BUTTON4_PRESSED) { // Scroll Up
            if (btn->on_scroll) btn->on_scroll(btn, 1, widget->user_data);
            return true;
        }
        if (bstate & BUTTON5_PRESSED) { // Scroll Down
             if (btn->on_scroll) btn->on_scroll(btn, -1, widget->user_data);
             return true;
        }
        
        // Stop bubbling if we are pressed?
        if (btn->is_pressed && !(bstate & BUTTON1_PRESSED)) {
             // Lost press (e.g. dragged out?)
             // For now keep simple.
        }
        
    } else if (event->type == TUI_EVENT_KEY) {
        if (event->data.key == '\n' || event->data.key == ' ') {
            // Simulate click
             if (btn->callback) btn->callback(btn, widget->user_data);
             return true;
        }
    }
    return false;
}

static void tui_button_free_impl(tui_widget_t* widget) {
    tui_button_t* btn = (tui_button_t*)widget;
    free(btn->text);
    free(btn);
}

tui_button_t* tui_button_create(const char* text, tui_rect_t bounds) {
    tui_button_t* btn = malloc(sizeof(tui_button_t));
    tui_widget_init(&btn->base, TUI_WIDGET_BUTTON);
    btn->base.bounds = bounds;
    btn->base.draw = tui_button_draw;
    btn->base.handle_event = tui_button_handle_event;
    btn->base.free = tui_button_free_impl;
    btn->base.focusable = true; // Buttons focusable by default
    
    btn->text = text ? strdup(text) : NULL;
    btn->callback = NULL;
    btn->on_right_click = NULL;
    btn->on_scroll = NULL;
    return btn;
}

void tui_button_set_click_callback(tui_button_t* button, tui_button_click_callback_t callback, void* user_data) {
    if (button) {
        button->callback = callback;
        button->base.user_data = user_data;
    }
}

void tui_button_set_right_click_callback(tui_button_t* button, tui_button_click_callback_t callback, void* user_data) {
     if (button) {
        button->on_right_click = callback;
        // user_data is shared in base? careful. 
        // Logic usually implies one user_data per widget.
        // If we want distinct user_data for right click, we might need a separate field or just reuse base.user_data.
        // The struct def for callback had 'void* user_data' param in typedef, usually passed from widget->user_data.
        // But here we might overwrite it.
        // Let's assume we stick to one user_data for the widget.
        if (user_data) button->base.user_data = user_data; 
    }
}

void tui_button_set_scroll_callback(tui_button_t* button, void (*callback)(tui_button_t*, int, void*), void* user_data) {
    if (button) {
        button->on_scroll = callback;
        if (user_data) button->base.user_data = user_data;
    }
}

// --- Label ---

static void tui_label_draw(tui_widget_t* widget) {
    tui_label_t* lbl = (tui_label_t*)widget;
    if (lbl->text) {
        mvprintw(widget->bounds.position.y, widget->bounds.position.x, "%s", lbl->text);
    }
}

static void tui_label_free_impl(tui_widget_t* widget) {
    tui_label_t* lbl = (tui_label_t*)widget;
    free(lbl->text);
    free(lbl);
}

tui_label_t* tui_label_create(const char* text, tui_point_t pos) {
    tui_label_t* lbl = malloc(sizeof(tui_label_t));
    tui_widget_init(&lbl->base, TUI_WIDGET_LABEL);
    lbl->base.bounds.position = pos;
    // Auto-size width?
    lbl->base.bounds.size.width = (int16_t)(text ? strlen(text) : 0);
    lbl->base.bounds.size.height = 1;
    
    lbl->base.draw = tui_label_draw;
    lbl->base.free = tui_label_free_impl;
    lbl->base.focusable = false;
    
    lbl->text = text ? strdup(text) : NULL;
    return lbl;
}

void tui_label_set_text(tui_label_t* label, const char* text) {
    if (label) {
        free(label->text);
        label->text = text ? strdup(text) : NULL;
        // Update size?
    }
}

// --- Panel ---

static void tui_panel_draw(tui_widget_t* widget) {
    tui_panel_t* p = (tui_panel_t*)widget;
    
    // Draw base container (children)
    // tui_container_draw(widget); // We should call this, but also draw border
    
    tui_rect_t b = widget->bounds;
    
    if (p->show_border) {
        // Use ncurses box
        // We need a specific WINDOW* for box() or use mvaddch manually
        // Drawing manually for now
        mvhline(b.position.y, b.position.x, 0, b.size.width);
        mvhline(b.position.y + b.size.height - 1, b.position.x, 0, b.size.width);
        mvvline(b.position.y, b.position.x, 0, b.size.height);
        mvvline(b.position.y, b.position.x + b.size.width - 1, 0, b.size.height);
        
        // Corners
        mvaddch(b.position.y, b.position.x, ACS_ULCORNER);
        mvaddch(b.position.y, b.position.x + b.size.width - 1, ACS_URCORNER);
        mvaddch(b.position.y + b.size.height - 1, b.position.x, ACS_LLCORNER);
        mvaddch(b.position.y + b.size.height - 1, b.position.x + b.size.width - 1, ACS_LRCORNER);
        
        if (p->title) {
             mvprintw(b.position.y, b.position.x + 2, " %s ", p->title);
        }
    }
    
    // Draw children
    tui_container_draw(widget);
}

static void tui_panel_free_impl(tui_widget_t* widget) {
    tui_panel_t* p = (tui_panel_t*)widget;
    // Free container resources manually since we can't call free_impl without freeing the struct too early
    tui_container_remove_all(&p->base);
    if(p->base.layout && p->base.layout->free) p->base.layout->free(p->base.layout);
    
    free(p->title);
    free(p);
}

tui_panel_t* tui_panel_create(tui_rect_t bounds, const char* title) {
    tui_panel_t* p = malloc(sizeof(tui_panel_t));
    // Init base container
    tui_widget_init(&p->base.base, TUI_WIDGET_PANEL);
    
    // Setup container parts manually since we didn't use tui_container_create
    p->base.first_child = NULL;
    p->base.last_child = NULL;
    p->base.child_count = 0;
    p->base.layout = NULL;
    p->base.auto_delete_children = true;

    p->base.base.bounds = bounds;
    p->base.base.draw = tui_panel_draw;
    p->base.base.handle_event = tui_container_handle_event;
    p->base.base.free = tui_panel_free_impl;
    
    p->show_border = true;
    p->title = title ? strdup(title) : NULL;
    return p;
}

// --- Input Field ---
// Enhanced Input Field with cursor and scrolling

static void tui_input_draw(tui_widget_t* widget) {
    tui_input_field_t* input = (tui_input_field_t*)widget;
    int attrs = (int)A_UNDERLINE;
    if (widget->has_focus) attrs |= (int)A_REVERSE;
    
    attron(attrs);
    tui_rect_t b = widget->bounds;
    
    // Fill background
    for(int i=0; i<b.size.width; i++) mvaddch(b.position.y, b.position.x + i, ' ');
    
    if (input->text) {
        int len = (int)strlen(input->text);
        int visible_width = b.size.width;
        
        // Calculate scroll offset primarily based on cursor position
        if (input->cursor_pos < input->scroll_offset) {
            input->scroll_offset = input->cursor_pos;
        } else if (input->cursor_pos >= input->scroll_offset + visible_width) {
            input->scroll_offset = input->cursor_pos - visible_width + 1;
        }
        
        // Clamp scroll offset
        if (input->scroll_offset < 0) input->scroll_offset = 0;
        
        int draw_len = len - input->scroll_offset;
        if (draw_len > visible_width) draw_len = visible_width;
        if (draw_len < 0) draw_len = 0;
        
        if (draw_len > 0) {
            mvprintw(b.position.y, b.position.x, "%.*s", draw_len, input->text + input->scroll_offset);
        }
    }
    
    // Cursor
    if (widget->has_focus) {
        int cursor_screen_x = b.position.x + (input->cursor_pos - input->scroll_offset);
        if (cursor_screen_x >= b.position.x && cursor_screen_x < b.position.x + b.size.width) {
            // If using standard cursor, we'd use move(). But we often hide it.
            // Let's use a distinct character or attribute if cursor is hidden.
            // Using a block or underscore.
            char c = ' ';
            if (input->text && input->cursor_pos < (int)strlen(input->text)) {
                c = input->text[input->cursor_pos];
            }
            mvaddch(b.position.y, cursor_screen_x, (chtype)c | (chtype)A_REVERSE | (chtype)A_BLINK);
        }
    }
    
    attroff(attrs);
}

static bool tui_input_handle_event(tui_widget_t* widget, const tui_event_t* event) {
    tui_input_field_t* input = (tui_input_field_t*)widget;
    
    if (event->type == TUI_EVENT_KEY) {
        int key = event->data.key;
        int len = input->text ? (int)strlen(input->text) : 0;
        
        if (key == KEY_BACKSPACE || key == 127 || key == KEY_DC) { // Delete/Backspace
             if (len > 0) {
                 if (key == KEY_DC) { // Delete key (right of cursor)
                     if (input->cursor_pos < len) {
                         memmove(input->text + input->cursor_pos, input->text + input->cursor_pos + 1, (size_t)(len - input->cursor_pos));
                     }
                 } else { // Backspace (left of cursor)
                     if (input->cursor_pos > 0) {
                         memmove(input->text + input->cursor_pos - 1, input->text + input->cursor_pos, (size_t)(len - input->cursor_pos + 1));
                         input->cursor_pos--;
                     }
                 }
                 tui_widget_draw(widget);
                 refresh();
                 return true;
             }
        } else if (key == KEY_LEFT) {
            if (input->cursor_pos > 0) input->cursor_pos--;
            tui_widget_draw(widget);
            refresh();
            return true;
        } else if (key == KEY_RIGHT) {
            if (input->cursor_pos < len) input->cursor_pos++;
            tui_widget_draw(widget);
            refresh();
            return true;
        } else if (key == KEY_HOME) {
            input->cursor_pos = 0;
            tui_widget_draw(widget);
            refresh();
            return true;
        } else if (key == KEY_END) {
            input->cursor_pos = len;
            tui_widget_draw(widget);
            refresh();
            return true;
        } else if (key >= 32 && key <= 126) { // Printable ASCII
            if (len < (int)input->max_length) {
                // Insert at cursor
                memmove(input->text + input->cursor_pos + 1, input->text + input->cursor_pos, (size_t)(len - input->cursor_pos + 1));
                input->text[input->cursor_pos] = (char)key;
                input->cursor_pos++;
                tui_widget_draw(widget);
                refresh();
            }
            return true;
        }
    }
    return false;
}

static void tui_input_field_free_impl(tui_widget_t* widget) {
    tui_input_field_t* input = (tui_input_field_t*)widget;
    free(input->text);
    free(input);
}

tui_input_field_t* tui_input_field_create(tui_rect_t bounds, size_t max_length) {
    tui_input_field_t* input = malloc(sizeof(tui_input_field_t));
    tui_widget_init(&input->base, TUI_WIDGET_INPUT);
    input->base.bounds = bounds;
    input->base.draw = tui_input_draw;
    input->base.handle_event = tui_input_handle_event;
    input->base.free = tui_input_field_free_impl;
    input->base.focusable = true;
    
    input->max_length = (int)max_length;
    input->text = calloc(max_length + 1, 1);
    input->cursor_pos = 0;
    input->scroll_offset = 0;
    input->password_mode = false;
    input->read_only = false;
    
    return input;
}

// --- List Widget ---

static void tui_list_draw(tui_widget_t* widget) {
    tui_list_t* list = (tui_list_t*)widget;
    tui_rect_t b = widget->bounds;
    
    // Clear area
    for(int y=0; y < b.size.height; y++) {
        mvhline(b.position.y + y, b.position.x, ' ', b.size.width);
    }
    
    if (!list->items || list->item_count == 0) {
        mvprintw(b.position.y, b.position.x, "[Empty]");
        return;
    }
    
    // Calculate visible range
    int max_visible = b.size.height;
    list->visible_items = max_visible;
    
    // Auto-scroll to keep selected in view
    if (list->selected_index < list->top_visible) {
        list->top_visible = list->selected_index;
    } else if (list->selected_index >= list->top_visible + max_visible) {
        list->top_visible = list->selected_index - max_visible + 1;
    }
    if (list->top_visible < 0) list->top_visible = 0;
    
    for (int i = 0; i < max_visible; i++) {
        int item_idx = list->top_visible + i;
        if (item_idx >= list->item_count) break;
        
        struct tui_list_item_t* item = list->items[item_idx];
        int y = b.position.y + i;
        
        int attrs = 0;
        if (item->selected || item_idx == list->selected_index) {
            attrs |= (int)A_REVERSE;
        }
        if (widget->has_focus && item_idx == list->selected_index) {
            attrs |= (int)A_BOLD; 
        }
        
        attron(attrs);
        mvhline(y, b.position.x, ' ', b.size.width); // fill bg
        // Clip text length
        int max_len = b.size.width - 2; // -1 padding, -1 safety
        if (max_len < 0) max_len = 0;
        
        mvprintw(y, b.position.x, " %.*s", max_len, item->text ? item->text : "");
        attroff(attrs);
    }
    
    // Scrollbar indicators?
    if (list->item_count > max_visible) {
        if (list->top_visible > 0) mvaddch(b.position.y, b.position.x + b.size.width - 1, ACS_UARROW);
        if (list->top_visible + max_visible < list->item_count) 
            mvaddch(b.position.y + b.size.height - 1, b.position.x + b.size.width - 1, ACS_DARROW);
    }
}

static bool tui_list_handle_event(tui_widget_t* widget, const tui_event_t* event) {
    tui_list_t* list = (tui_list_t*)widget;
    
    if (event->type == TUI_EVENT_KEY) {
        int key = event->data.key;
        if (key == KEY_UP) {
            if (list->selected_index > 0) {
                list->selected_index--;
                if (list->on_select) list->on_select(list, list->selected_index, widget->user_data);
                tui_widget_draw(widget);
                refresh();
            }
            return true;
        } else if (key == KEY_DOWN) {
             if (list->selected_index < list->item_count - 1) {
                list->selected_index++;
                if (list->on_select) list->on_select(list, list->selected_index, widget->user_data);
                tui_widget_draw(widget);
                refresh();
            }
            return true;
        } else if (key == KEY_PPAGE) { // Page Up
             list->selected_index -= list->visible_items;
             if (list->selected_index < 0) list->selected_index = 0;
             tui_widget_draw(widget);
             refresh();
             return true;
        } else if (key == KEY_NPAGE) { // Page Down
             list->selected_index += list->visible_items;
             if (list->selected_index >= list->item_count) list->selected_index = list->item_count - 1;
             tui_widget_draw(widget);
             refresh();
             return true;
        } else if (key == '\n' || key == ' ') {
            // Trigger selection action?
             if (list->on_select) list->on_select(list, list->selected_index, widget->user_data);
             return true;
        }
    } else if (event->type == TUI_EVENT_MOUSE) {
        if (event->data.mouse.button & (BUTTON4_PRESSED)) { // Scroll Up
             list->selected_index -= 1;
             if (list->selected_index < 0) list->selected_index = 0;
             tui_widget_draw(widget);
             refresh();
             return true;
        }
        if (event->data.mouse.button & (BUTTON5_PRESSED)) { // Scroll Down
             list->selected_index += 1;
             if (list->selected_index >= list->item_count) list->selected_index = list->item_count - 1;
             tui_widget_draw(widget);
             refresh();
             return true;
        }
        
        if (event->data.mouse.pressed) {
             // Calculate clicked index
             int rel_y = event->data.mouse.y - widget->bounds.position.y;
             int idx = list->top_visible + rel_y;
             if (idx >= 0 && idx < list->item_count) {
                 list->selected_index = idx;
                 if (list->on_select) list->on_select(list, list->selected_index, widget->user_data);
                 tui_widget_draw(widget);
                 refresh();
                 return true;
             }
        }
    }
    return false;
}

static void tui_list_free_impl(tui_widget_t* widget) {
    tui_list_t* list = (tui_list_t*)widget;
    if (list->items) {
        for (int i = 0; i < list->item_count; i++) {
             free(list->items[i]->text);
             free(list->items[i]);
        }
        free(list->items);
    }
    free(list);
}

tui_list_t* tui_list_create(tui_rect_t bounds) {
    tui_list_t* list = malloc(sizeof(tui_list_t));
    tui_widget_init(&list->base, TUI_WIDGET_LIST);
    list->base.bounds = bounds;
    list->base.draw = tui_list_draw;
    list->base.handle_event = tui_list_handle_event;
    list->base.free = tui_list_free_impl;
    list->base.focusable = true;
    
    list->items = NULL;
    list->item_count = 0;
    list->selected_index = -1;
    list->top_visible = 0;
    list->visible_items = 0;
    list->multi_select = false;
    list->on_select = NULL;
    
    return list;
}

bool tui_list_add_item(tui_list_t* list, const char* text) {
    if (!list) return false;
    
    struct tui_list_item_t* item = malloc(sizeof(struct tui_list_item_t));
    item->text = strdup(text);
    item->selected = false;
    item->disabled = false;
    item->user_data = NULL;
    
    list->items = realloc(list->items, sizeof(struct tui_list_item_t*) * (size_t)(list->item_count + 1));
    list->items[list->item_count] = item;
    list->item_count++;
    
    if (list->selected_index == -1) list->selected_index = 0; // Auto select first
    
    return true;
}

void tui_list_set_select_callback(tui_list_t* list, tui_list_select_callback_t callback, void* user_data) {
    if (list) {
        list->on_select = callback;
        list->base.user_data = user_data;
    }
}


// --- Checkbox ---

static void tui_checkbox_draw(tui_widget_t* widget) {
    tui_checkbox_t* cb = (tui_checkbox_t*)widget;
    tui_rect_t b = widget->bounds;
    
    char state_char = ' ';
    if (cb->state == 1 || (cb->state == 0 && cb->checked)) state_char = 'X';
    else if (cb->state == 2) state_char = '-'; // Indeterminate
    
    // Draw box [X]
    int y = b.position.y;
    int x = b.position.x;
    
    if (widget->has_focus) attron(A_BOLD);
    mvprintw(y, x, "[%c] %s", state_char, cb->text ? cb->text : "");
    if (widget->has_focus) attroff(A_BOLD);
}

static bool tui_checkbox_handle_event(tui_widget_t* widget, const tui_event_t* event) {
    tui_checkbox_t* cb = (tui_checkbox_t*)widget;
    bool triggered = false;
    
    if (event->type == TUI_EVENT_MOUSE) {
        if (event->data.mouse.button & (BUTTON1_CLICKED | BUTTON1_RELEASED)) {
            if (tui_widget_contains_point(widget, (tui_point_t){(int16_t)event->data.mouse.x, (int16_t)event->data.mouse.y})) {
                triggered = true;
            }
        }
    } else if (event->type == TUI_EVENT_KEY) {
        if (event->data.key == ' ' || event->data.key == '\n') {
            triggered = true;
        }
    }
    
    if (triggered) {
        cb->checked = !cb->checked;
        cb->state = cb->checked ? 1 : 0;
        if (cb->on_toggle) cb->on_toggle(cb, cb->checked, widget->user_data);
        tui_widget_draw(widget);
        refresh();
        return true;
    }
    return false;
}

static void tui_checkbox_free_impl(tui_widget_t* widget) {
    tui_checkbox_t* cb = (tui_checkbox_t*)widget;
    free(cb->text);
    free(cb);
}

tui_checkbox_t* tui_checkbox_create(const char* text, tui_rect_t bounds) {
    tui_checkbox_t* cb = malloc(sizeof(tui_checkbox_t));
    tui_widget_init(&cb->base, TUI_WIDGET_CUSTOM); // TUI_WIDGET_CHECKBOX if defined
    cb->base.bounds = bounds;
    cb->base.draw = tui_checkbox_draw;
    cb->base.handle_event = tui_checkbox_handle_event;
    cb->base.free = tui_checkbox_free_impl;
    cb->base.focusable = true;
    
    cb->text = text ? strdup(text) : NULL;
    cb->checked = false;
    cb->tristate = false;
    cb->state = 0;
    cb->on_toggle = NULL;
    
    return cb;
}

void tui_checkbox_set_toggle_callback(tui_checkbox_t* checkbox, tui_checkbox_toggle_callback_t callback, void* user_data) {
    if(checkbox) {
        checkbox->on_toggle = callback;
        checkbox->base.user_data = user_data;
    }
}

// --- Radio Button ---
// Simplified group logic: application handles exclusion for now, or we iterate parent siblings?
// TUI approach: The container or logic usually handles mutex. We'll just provide the widget.

static void tui_radio_draw(tui_widget_t* widget) {
    tui_radio_button_t* rb = (tui_radio_button_t*)widget;
    tui_rect_t b = widget->bounds;
    
    char state_char = rb->selected ? '*' : ' ';
    
    int y = b.position.y;
    int x = b.position.x;
    
    if (widget->has_focus) attron(A_BOLD);
    mvprintw(y, x, "(%c) %s", state_char, rb->text ? rb->text : "");
    if (widget->has_focus) attroff(A_BOLD);
}

static bool tui_radio_handle_event(tui_widget_t* widget, const tui_event_t* event) {
    tui_radio_button_t* rb = (tui_radio_button_t*)widget;
    bool triggered = false;
    
    if (event->type == TUI_EVENT_MOUSE) {
        if (event->data.mouse.button & (BUTTON1_CLICKED | BUTTON1_RELEASED)) {
             if (tui_widget_contains_point(widget, (tui_point_t){(int16_t)event->data.mouse.x, (int16_t)event->data.mouse.y})) {
                triggered = true;
            }
        }
    } else if (event->type == TUI_EVENT_KEY) {
        if (event->data.key == ' ' || event->data.key == '\n') {
            triggered = true;
        }
    }
    
    if (triggered && !rb->selected) {
        rb->selected = true;
        if (rb->on_select) rb->on_select(rb, rb->group_id, widget->user_data);
        tui_widget_draw(widget);
        refresh();
        return true;
    }
    return false;
}

static void tui_radio_free_impl(tui_widget_t* widget) {
    tui_radio_button_t* rb = (tui_radio_button_t*)widget;
    free(rb->text);
    free(rb);
}

tui_radio_button_t* tui_radio_button_create(const char* text, tui_rect_t bounds, int group_id) {
    tui_radio_button_t* rb = malloc(sizeof(tui_radio_button_t));
    tui_widget_init(&rb->base, TUI_WIDGET_CUSTOM);
    rb->base.bounds = bounds;
    rb->base.draw = tui_radio_draw;
    rb->base.handle_event = tui_radio_handle_event;
    rb->base.free = tui_radio_free_impl;
    rb->base.focusable = true;
    
    rb->text = text ? strdup(text) : NULL;
    rb->selected = false;
    rb->group_id = group_id;
    rb->on_select = NULL;
    
    return rb;
}

void tui_radio_button_set_select_callback(tui_radio_button_t* radio, tui_radio_button_select_callback_t callback, void* user_data) {
     if(radio) {
        radio->on_select = callback;
        radio->base.user_data = user_data;
    }
}

// --- Tab Container ---

static void tui_tab_container_draw(tui_widget_t* widget) {
    tui_tab_container_t* tabs = (tui_tab_container_t*)widget;
    tui_rect_t b = widget->bounds;
    
    // Draw Tab Header
    int y = b.position.y;
    int x = b.position.x;
    
    // Simple top bar
    mvhline(y, x, ' ', b.size.width); // Clear header line
    
    int curr_x = x;
    for(int i=0; i<tabs->tab_count; i++) {
        tui_tab_t* t = tabs->tabs[i];
        int label_len = (int)strlen(t->title) + 2; // " Title "
        
        if (i == tabs->selected_tab) {
            attron(A_REVERSE | A_BOLD);
        }
        mvprintw(y, curr_x, " %s ", t->title);
        if (i == tabs->selected_tab) {
            attroff(A_REVERSE | A_BOLD);
        }
        
        // Draw separator
        curr_x += label_len;
        if (i < tabs->tab_count -1) mvaddch(y, curr_x++, '|');
        else mvaddch(y, curr_x++, ' ');
    }
    
    // Draw content border/separator
    mvhline(y+1, x, ACS_HLINE, b.size.width);
    
    // Draw selected content
    if (tabs->selected_tab >= 0 && tabs->selected_tab < tabs->tab_count) {
        tui_widget_t* content = tabs->tabs[tabs->selected_tab]->content;
        if (content) {
            tui_widget_draw(content);
        }
    }
}

static bool tui_tab_container_handle_event(tui_widget_t* widget, const tui_event_t* event) {
    tui_tab_container_t* tabs = (tui_tab_container_t*)widget;
    
    // Handle switching tabs with keys (Left/Right) if tab header focused? 
    // Or just pass to content?
    // For now, let's implement mouse clicking on tabs.
    
    if (event->type == TUI_EVENT_MOUSE && (event->data.mouse.button & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_RELEASED))) {
        // Check if click is in header row
        if (event->data.mouse.y == widget->bounds.position.y) {
            int mx = event->data.mouse.x;
            int x = widget->bounds.position.x;
            
             if (event->data.mouse.button & (BUTTON1_CLICKED | BUTTON1_RELEASED)) {
                // Hit test tabs
                int curr_x = x;
                for(int i=0; i<tabs->tab_count; i++) {
                    int label_len = (int)strlen(tabs->tabs[i]->title) + 2; // " Title "
                    int next_x = curr_x + label_len + (i < tabs->tab_count-1 ? 1 : 1);
                    
                    if (mx >= curr_x && mx < next_x) {
                        tabs->selected_tab = i;
                        tui_widget_draw(widget);
                        clear(); // Hack: Force full clear because switching tabs might leave garbage from old tab
                        tui_widget_t* root = tui_get_root();
                        if (root) tui_widget_draw(root); // Redraw everything
                        
                        // Set focus to the first focusable child of the new tab?
                        tui_widget_t* content = tabs->tabs[i]->content;
                        // Simplistic focus transfer (could recurse to find first input)
                        // For now, if content is list, focus it.
                        if (content && content->type == TUI_WIDGET_LIST) {
                             tui_set_focus(content);
                        } else if (content && content->type == TUI_WIDGET_CUSTOM) {
                             // Panel... try to find child?
                             // tui_container_focus_first(content); // Not implemented yet
                        }
                        
                        refresh();
                        return true;
                    }
                    curr_x = next_x;
                }
             }
             return true; // Consumed event in header
        }
    }
    
    // If not header, pass to active content
    
    // Pass event to active content if not handled by header
    if (tabs->selected_tab >= 0 && tabs->selected_tab < tabs->tab_count) {
        tui_widget_t* content = tabs->tabs[tabs->selected_tab]->content;
        if (content) {
             // Handle TAB key navigation between children manually? 
             // Or rely on container logic.
             // Usually focus is global. If focus is within content, it handles keys.
             // If event is Mouse, we just pass it.
             return tui_widget_handle_event(content, event);
        }
    }
    
    return false;
}

static void tui_tab_container_free_impl(tui_widget_t* widget) {
    tui_tab_container_t* tabs = (tui_tab_container_t*)widget;
    for(int i=0; i<tabs->tab_count; i++) {
         free(tabs->tabs[i]->title);
         // Do we free content? "tui_container" usually owns children.
         // Let's assume we own content and free it.
         tui_widget_destroy(tabs->tabs[i]->content); 
         free(tabs->tabs[i]);
    }
    free(tabs->tabs);
    free(tabs);
}

tui_tab_container_t* tui_tab_container_create(tui_rect_t bounds) {
    tui_tab_container_t* tabs = malloc(sizeof(tui_tab_container_t));
    tui_widget_init(&tabs->base.base, TUI_WIDGET_CUSTOM);
    tabs->base.base.bounds = bounds;
    tabs->base.base.draw = tui_tab_container_draw;
    tabs->base.base.handle_event = tui_tab_container_handle_event;
    tabs->base.base.free = tui_tab_container_free_impl;
    
    // Initialize container parts
    tabs->base.first_child = NULL;
    tabs->base.last_child = NULL;
    tabs->base.child_count = 0;
    tabs->base.layout = NULL;
    tabs->base.auto_delete_children = true;
    
    tabs->tabs = NULL;
    tabs->tab_count = 0;
    tabs->selected_tab = -1;
    tabs->tab_position = 0;
    
    return tabs;
}

int tui_tab_container_add_tab(tui_tab_container_t* tabs, const char* title, tui_widget_t* content) {
    if (!tabs) return -1;
    
    tabs->tabs = realloc(tabs->tabs, sizeof(tui_tab_t*) * (size_t)(tabs->tab_count + 1));
    tui_tab_t* tab = malloc(sizeof(tui_tab_t));
    tab->title = strdup(title);
    tab->content = content;
    tab->enabled = true;
    
    if (content) {
        // Resize content to fit container body (taking header into account)
        tui_rect_t body = tabs->base.base.bounds;
        body.position.y += 2; // Header + separator
        body.size.height -= 2;
        body.position.x += 1; // Margin
        body.size.width -= 2;
        tui_widget_set_bounds(content, body);
        content->parent = (tui_widget_t*)tabs;
    }
    
    tabs->tabs[tabs->tab_count] = tab;
    int idx = tabs->tab_count;
    tabs->tab_count++;
    
    if (tabs->selected_tab == -1) tabs->selected_tab = 0;
    
    return idx;
}
