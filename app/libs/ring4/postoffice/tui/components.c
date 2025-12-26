#include "components.h"
#include <tui/tui.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <math.h>

// --- Graph ---

static void tui_graph_draw(tui_widget_t* widget) {
    tui_graph_t* g = (tui_graph_t*)widget;
    tui_rect_t b = widget->bounds;
    
    // Draw box/border
    // mvhline...
    
    if (g->count < 2) return;
    
    // Auto scale
    float min = g->min_val;
    float max = g->max_val;
    
    if (g->auto_scale) {
        min = g->values[0];
        max = g->values[0];
        for(int i=1; i<g->count; i++) {
            if(g->values[i] < min) min = g->values[i];
            if(g->values[i] > max) max = g->values[i];
        }
    }
    
    if (max == min) max = min + 1.0f;
    float range = max - min;
    
    // Plot
    // Simple block character set:  ▂▃▄▅▆▇█
    const char* blocks[] = {" ", " ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    
    int graph_w = b.size.width - 2;
    int graph_h = b.size.height - 2;
    
    int start_idx = (g->count > graph_w) ? (g->count - graph_w) : 0;
    
    for (int x = 0; x < graph_w; x++) {
        int idx = start_idx + x;
        if (idx >= g->count) break;
        
        float v = g->values[idx];
        float normalized = (v - min) / range; // 0.0 to 1.0
        
        // Height in blocks
        // int h_blocks = (int)(normalized * (float)graph_h * 8.0f); // Unused
        // Standard terminal chart:
        int bar_h = (int)(normalized * (float)graph_h);
        if(bar_h < 0) bar_h = 0;
        if(bar_h >= graph_h) bar_h = graph_h - 1;
        
        // We can use braille or blocks. Blocks are easier.
        // Draw column x
        int screen_x = b.position.x + 1 + x;
        
        for (int y = 0; y <= bar_h; y++) {
            // Determine char
            const char* c = blocks[8];
            if (y == bar_h) {
                // Determine fractional part for top block
                float remainder = (normalized * (float)graph_h) - (float)bar_h;
                int b_idx = (int)(remainder * 8.0f);
                if(b_idx > 8) b_idx = 8;
                c = blocks[b_idx];
            }
            
            mvprintw(b.position.y + b.size.height - 2 - y, screen_x, "%s", c);
        }
    }
    
    // Draw title/label?
}

static void tui_graph_free(tui_widget_t* w) {
    tui_graph_t* g = (tui_graph_t*)w;
    free(g->values);
    free(g);
}

tui_graph_t* tui_graph_create(tui_rect_t bounds) {
    tui_graph_t* g = malloc(sizeof(tui_graph_t));
    tui_widget_init(&g->base, TUI_WIDGET_CUSTOM);
    g->base.bounds = bounds;
    g->base.draw = tui_graph_draw;
    g->base.free = tui_graph_free;
    
    g->values = NULL;
    g->count = 0;
    g->capacity = 0;
    g->auto_scale = true;
    g->min_val = 0;
    g->max_val = 100;
    
    return g;
}

void tui_graph_add_value(tui_graph_t* graph, float value) {
    if (graph->count >= graph->capacity) {
        graph->capacity = (graph->capacity == 0) ? 64 : graph->capacity * 2;
        graph->values = realloc(graph->values, sizeof(float) * (size_t)graph->capacity);
    }
    graph->values[graph->count++] = value;
}

// --- Gauge ---

static void tui_gauge_draw(tui_widget_t* w) {
    tui_gauge_t* g = (tui_gauge_t*)w;
    tui_rect_t b = w->bounds;
    
    // Horizontal Bar
    // [||||||||||      ] 50%
    
    float pct = g->value / g->max_value;
    if(pct < 0) pct = 0;
    if(pct > 1) pct = 1;
    
    int bar_w = b.size.width - 2;
    int fill_w = (int)(pct * (float)bar_w);
    
    // Color
    // int color = 0; // Unused
    // Use ncurses colors if init...
    
    // Clear background
    for(int y=0; y < b.size.height; y++) {
        mvhline(b.position.y + y, b.position.x, ' ', b.size.width);
    }
    
    mvaddch(b.position.y, b.position.x, '[');
    
    attron(A_REVERSE);
    for(int i=0; i<fill_w; i++) mvaddch(b.position.y, b.position.x + 1 + i, ' ');
    attroff(A_REVERSE);
    
    for(int i=fill_w; i<bar_w; i++) mvaddch(b.position.y, b.position.x + 1 + i, ' ');
    
    mvaddch(b.position.y, b.position.x + b.size.width - 1, ']');
    
    // Label overlap?
    if (g->label) {
        mvprintw(b.position.y, b.position.x + 1, "%s", g->label);
    } else {
        mvprintw(b.position.y, b.position.x + 1, "%.1f%%", (double)(pct * 100.0f));
    }
}

static void tui_gauge_free(tui_widget_t* w) {
    tui_gauge_t* g = (tui_gauge_t*)w;
    free(g->label);
    free(g);
}

tui_gauge_t* tui_gauge_create(tui_rect_t bounds, float max) {
    tui_gauge_t* g = malloc(sizeof(tui_gauge_t));
    tui_widget_init(&g->base, TUI_WIDGET_CUSTOM);
    g->base.bounds = bounds;
    g->base.draw = tui_gauge_draw;
    g->base.free = tui_gauge_free; 
    
    g->value = 0;
    g->max_value = max;
    g->label = NULL;
    return g;
}

void tui_gauge_set_value(tui_gauge_t* gauge, float value) {
    gauge->value = value;
}

void tui_gauge_set_label(tui_gauge_t* gauge, const char* label) {
    free(gauge->label);
    gauge->label = label ? strdup(label) : NULL;
}
