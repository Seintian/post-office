#ifndef TUI_COMPONENTS_H
#define TUI_COMPONENTS_H

#include <tui/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Components (Visualizations) ---

/**
 * @struct tui_graph_t
 * @brief Line or Bar graph data visualization
 */
typedef struct tui_graph_t {
    tui_widget_t base;
    
    float* values;
    int count;
    int capacity;
    
    float min_val;
    float max_val;
    bool auto_scale;
    
    tui_color_pair_t color;
    char style_char; // if 0, use block characters
} tui_graph_t;

/**
 * @struct tui_gauge_t
 * @brief Progress or Meter gauge (horizontal/vertical)
 */
typedef struct tui_gauge_t {
    tui_widget_t base;
    
    float value; // 0.0 to 1.0 (or max)
    float max_value;
    
    tui_color_pair_t color_low;
    tui_color_pair_t color_medium;
    tui_color_pair_t color_high;
    
    char* label;
} tui_gauge_t;

// API
tui_graph_t* tui_graph_create(tui_rect_t bounds);
void tui_graph_add_value(tui_graph_t* graph, float value);
void tui_graph_set_data(tui_graph_t* graph, float* values, int count);

tui_gauge_t* tui_gauge_create(tui_rect_t bounds, float max);
void tui_gauge_set_value(tui_gauge_t* gauge, float value);
void tui_gauge_set_label(tui_gauge_t* gauge, const char* label);

#ifdef __cplusplus
}
#endif

#endif // TUI_COMPONENTS_H
