/**
 * @file layout.c
 * @brief TUI Layout Engine Implementation
 */

#include "layout.h"
#include "widgets.h"
#include "types.h"
#include "tui/tui.h"
#include <stdlib.h>
#include <string.h>

// --- Helper Functions ---

void tui_layout_params_init(tui_layout_params_t* params) {
    if (!params) return;
    memset(params, 0, sizeof(tui_layout_params_t));
    params->h_align = TUI_ALIGN_LEFT;
    params->v_align = TUI_ALIGN_TOP;
}

void tui_layout_params_set_margin(tui_layout_params_t* params, int left, int top, int right, int bottom) {
    if(params) { params->margin.left=left; params->margin.top=top; params->margin.right=right; params->margin.bottom=bottom; }
}
void tui_layout_params_set_padding(tui_layout_params_t* params, int left, int top, int right, int bottom) {
    if(params) { params->padding.left=left; params->padding.top=top; params->padding.right=right; params->padding.bottom=bottom; }
}
void tui_layout_params_set_size_hint(tui_layout_params_t* params, int width, int height) {
    if(params) { params->min_width=width; params->min_height=height; }
}
void tui_layout_params_set_min_size(tui_layout_params_t* params, int width, int height) {
    if(params) { params->min_width=width; params->min_height=height; }
}
void tui_layout_params_set_max_size(tui_layout_params_t* params, int width, int height) {
    if(params) { params->max_width=width; params->max_height=height; }
}
void tui_layout_params_set_expand(tui_layout_params_t* params, bool expand_x, bool expand_y) {
    if(params) { params->expand_x=expand_x; params->expand_y=expand_y; }
}
void tui_layout_params_set_fill(tui_layout_params_t* params, bool fill_x, bool fill_y) {
    if(params) { params->fill_x=fill_x; params->fill_y=fill_y; }
}
void tui_layout_params_set_alignment(tui_layout_params_t* params, tui_horizontal_alignment_t h, tui_vertical_alignment_t v) {
    if(params) { params->h_align=h; params->v_align=v; }
}

tui_size_t tui_size_apply_margins(tui_size_t size, const tui_layout_params_t* params) {
    if (!params) return size;
    size.width += (int16_t)(params->margin.left + params->margin.right);
    size.height += (int16_t)(params->margin.top + params->margin.bottom);
    return size;
}
tui_rect_t tui_rect_apply_margins(tui_rect_t rect, const tui_layout_params_t* params) {
    if (!params) return rect;
    rect.position.x += (int16_t)params->margin.left;
    rect.position.y += (int16_t)params->margin.top;
    rect.size.width -= (int16_t)(params->margin.left + params->margin.right);
    rect.size.height -= (int16_t)(params->margin.top + params->margin.bottom);
    if (rect.size.width < 0) rect.size.width = 0;
    if (rect.size.height < 0) rect.size.height = 0;
    return rect;
}

tui_rect_t tui_rect_apply_padding(tui_rect_t rect, const tui_layout_params_t* params) {
    if (!params) return rect;
    rect.position.x += (int16_t)params->padding.left;
    rect.position.y += (int16_t)params->padding.top;
    rect.size.width -= (int16_t)(params->padding.left + params->padding.right);
    rect.size.height -= (int16_t)(params->padding.top + params->padding.bottom);
    if (rect.size.width < 0) rect.size.width = 0;
    if (rect.size.height < 0) rect.size.height = 0;
    return rect;
}

// --- Layout Logic Helpers ---

// Helper to align a child within an allocated slot
static void align_child_in_slot(tui_widget_t* child, tui_rect_t slot) {
    if (!child) return;
    tui_layout_params_t* p = &child->layout_params;
    
    // Apply margins from slot
    tui_rect_t content_area = tui_rect_apply_margins(slot, p);
    
    tui_size_t child_size = content_area.size;
    
    // Check constraints
    if (p->min_width > 0 && child_size.width < p->min_width) child_size.width = (int16_t)p->min_width;
    if (p->max_width > 0 && child_size.width > p->max_width) child_size.width = (int16_t)p->max_width;
    if (p->min_height > 0 && child_size.height < p->min_height) child_size.height = (int16_t)p->min_height;
    if (p->max_height > 0 && child_size.height > p->max_height) child_size.height = (int16_t)p->max_height;
    
    // Calculate position based on alignment
    tui_point_t pos = content_area.position;
    
    if (child_size.width < content_area.size.width) {
        if (p->h_align == TUI_ALIGN_CENTER) pos.x += (int16_t)((content_area.size.width - child_size.width) / 2);
        else if (p->h_align == TUI_ALIGN_RIGHT) pos.x += (int16_t)(content_area.size.width - child_size.width);
    }
    
    if (child_size.height < content_area.size.height) {
        if (p->v_align == TUI_ALIGN_MIDDLE) pos.y += (int16_t)((content_area.size.height - child_size.height) / 2);
        else if (p->v_align == TUI_ALIGN_BOTTOM) pos.y += (int16_t)(content_area.size.height - child_size.height);
    }
    
    tui_rect_t final_bounds = {pos, child_size};
    tui_widget_set_bounds(child, final_bounds);
}


// --- Box Layout ---

typedef struct {
    tui_orientation_t orientation;
    int spacing;
} box_layout_data_t;

static void box_layout_fn(tui_layout_manager_t* self, tui_widget_t* container_widget) {
    if (!self || !container_widget) return;
    tui_container_t* container = (tui_container_t*)container_widget;
    box_layout_data_t* data = (box_layout_data_t*)self->data;
    
    tui_rect_t bounds = container_widget->bounds;
    // Apply container padding
    // We access the widget's layout params directly.
    tui_layout_params_t* cp = &container_widget->layout_params; 
    bounds = tui_rect_apply_padding(bounds, cp);
    
    // Calculate total fixed size and total weight
    int total_fixed = 0;
    float total_weight = 0;
    int count = 0;
    
    tui_widget_t* child = container->first_child;
    while (child) {
        if (child->visible) {
            count++;
            tui_layout_params_t* p = &child->layout_params;
            float weight = (data->orientation == TUI_ORIENTATION_HORIZONTAL) ? p->weight_x : p->weight_y;
            if (weight > 0) {
                total_weight += weight;
            } else {
                // Fixed size (use min or current?)
                // Usually we ask the widget for preferred size, but we don't have a robust measure system yet.
                // We'll trust min_width/height or a default.
                int size = (data->orientation == TUI_ORIENTATION_HORIZONTAL) ? p->min_width : p->min_height;
                if (size == 0) size = (data->orientation == TUI_ORIENTATION_HORIZONTAL) ? 10 : 3; // Default fallback
                total_fixed += size;
            }
        }
        child = child->next;
    }
    
    if (count == 0) return;
    
    int total_spacing = (count - 1) * data->spacing;
    int available = (data->orientation == TUI_ORIENTATION_HORIZONTAL) ? bounds.size.width : bounds.size.height;
    int remaining = available - total_fixed - total_spacing;
    
    if (remaining < 0) remaining = 0;
    
    // Distribute
    int current_offset = 0;
    child = container->first_child;
    while (child) {
        if (!child->visible) {
            child = child->next;
            continue;
        }
        
        tui_layout_params_t* p = &child->layout_params;
        float weight = (data->orientation == TUI_ORIENTATION_HORIZONTAL) ? p->weight_x : p->weight_y;
        
        int slot_size = 0;
        if (weight > 0) {
            slot_size = (int)((float)remaining * (weight / total_weight));
        } else {
            slot_size = (data->orientation == TUI_ORIENTATION_HORIZONTAL) ? p->min_width : p->min_height;
            if (slot_size == 0) slot_size = (data->orientation == TUI_ORIENTATION_HORIZONTAL) ? 10 : 3;
        }
        
        tui_rect_t slot;
        if (data->orientation == TUI_ORIENTATION_HORIZONTAL) {
            slot.position.x = (int16_t)(bounds.position.x + current_offset);
            slot.position.y = bounds.position.y;
            slot.size.width = (int16_t)slot_size;
            slot.size.height = bounds.size.height;
        } else {
            slot.position.x = bounds.position.x;
            slot.position.y = (int16_t)(bounds.position.y + current_offset);
            slot.size.width = bounds.size.width;
            slot.size.height = (int16_t)slot_size;
        }
        
        align_child_in_slot(child, slot);
        
        current_offset += slot_size + data->spacing;
        child = child->next;
    }
}

static void box_layout_free(tui_layout_manager_t* self) {
    if (self) {
        free(self->data);
        free(self);
    }
}

tui_layout_manager_t* tui_create_box_layout(tui_orientation_t orientation, int spacing) {
    tui_layout_manager_t* lm = malloc(sizeof(tui_layout_manager_t));
    box_layout_data_t* data = malloc(sizeof(box_layout_data_t));
    data->orientation = orientation;
    data->spacing = spacing;
    
    lm->data = data;
    lm->layout = box_layout_fn; // The implementation function
    lm->free = box_layout_free;
    lm->measure = NULL; // Optional
    return lm;
}

// --- Stack Layout ---

static void stack_layout_fn(tui_layout_manager_t* self, tui_widget_t* container_widget) {
    if (!self || !container_widget) return;
    tui_container_t* container = (tui_container_t*)container_widget;
    
    tui_widget_t* child = container->first_child;
    while (child) {
        if (!child->visible) {
            child = child->next;
            continue;
        }
        // Stack layout simply fills the container for each child (respecting child's params/margins)
        align_child_in_slot(child, container_widget->bounds);
        child = child->next;
    }
}

static void stack_layout_free(tui_layout_manager_t* self) {
    if (self) {
        free(self);
    }
}

tui_layout_manager_t* tui_layout_stack_create(void) {
    tui_layout_manager_t* lm = malloc(sizeof(tui_layout_manager_t));
    lm->data = NULL;
    lm->layout = stack_layout_fn;
    lm->free = stack_layout_free;
    lm->measure = NULL;
    return lm;
}


// --- API Wrappers ---

tui_layout_manager_t* tui_layout_box_create(tui_orientation_t orientation, int spacing) {
    return tui_create_box_layout(orientation, spacing);
}

void tui_container_set_layout(struct tui_container_t* container, tui_layout_manager_t* layout) {
    if (container->layout && container->layout->free) {
        container->layout->free(container->layout);
    }
    container->layout = layout;
}

void tui_widget_set_layout(tui_widget_t* widget, tui_layout_manager_t* layout) {
    // Basic wrapper, assumes widget is container or has layout capabilities
    // Since we don't have RTTI in C easily without the type enum, we trust caller or check type
    // But layout is stored in container struct usually.
    // widgets.h has tui_container_set_layout. This is likely alias or general version.
    if (widget->type == TUI_WIDGET_PANEL || (widget->type >= TUI_WIDGET_BASE && widget->type <= TUI_WIDGET_CUSTOM)) { // Loose check
        // Assuming it's a container structure compatible
        tui_container_t* c = (tui_container_t*)widget;
        tui_container_set_layout(c, layout);
    }
}

void tui_widget_layout_update(tui_widget_t* widget) {
    if (!widget) return;
    
    // If widget has a layout manager, run it
    if (widget->type == TUI_WIDGET_PANEL || widget->type == TUI_WIDGET_WINDOW || widget->type == TUI_WIDGET_TAB_CONTAINER) {
         tui_container_t* c = (tui_container_t*)widget;
         if (c->layout && c->layout->layout) {
             c->layout->layout(c->layout, widget);
         }
         
         // Recursively update children
         tui_widget_t* child = c->first_child;
         while (child) {
             tui_widget_t* next = child->next;
             tui_widget_layout_update(child);
             child = next;
         }
    }
}
