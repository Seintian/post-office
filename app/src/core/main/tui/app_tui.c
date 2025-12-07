#include "app_tui.h"
#include <postoffice/tui/tui.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "app_tui.h"
#include <postoffice/tui/tui.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// --- Callbacks ---

static void on_quit(tui_button_t* btn, void* data) {
    (void)btn; (void)data;
    tui_quit();
}

static void on_add_process(tui_button_t* btn, void* data) {
    (void)btn; 
    tui_table_t* table = (tui_table_t*)data;
    char pid[32], cpu[32], mem[32];
    sprintf(pid, "%d", rand() % 9999 + 1000);
    sprintf(cpu, "%.1f%%", (double)((float)(rand() % 1000) / 10.0f));
    sprintf(mem, "%d MB", rand() % 4096);
    const char* row[] = {"nginx", pid, "running", cpu, mem};
    tui_table_add_row(table, row);
    tui_widget_draw((tui_widget_t*)table);
    // Force immediate screen update so it doesn't feel like "doing nothing"
    tui_render();
}

static void on_checkbox_toggle(tui_checkbox_t* cb, bool checked, void* data) {
    (void)cb; (void)checked; (void)data;
    // Optional: Log or react
}

// static void on_radio_select(tui_radio_button_t* rb, int group, void* data) { ... }
static void on_radio_select(tui_radio_button_t* rb, int group, void* data) {
    (void)rb; (void)data; (void)group;
}

// --- Tab Creators ---

static tui_widget_t* create_dashboard_tab(tui_rect_t bounds, tui_graph_t** out_graph, tui_gauge_t** out_gauge) {
    tui_panel_t* panel = tui_panel_create(bounds, "System Overview");
    // tui_rect_t content_r = bounds; // Unused 
    // Panel usually draws border inside bounds, children should be offset.
    // For simplicity, we manually layout children relative to (0,0) of parent if we had relative layout,
    // but TUI uses absolute coordinates currently.
    // We must pass absolute coordinates to children.
    
    int w = bounds.size.width;
    int h = bounds.size.height;
    int x = bounds.position.x + 1;
    int y = bounds.position.y + 1;
    w -= 2; h -= 2; // Border inset

    // Left: Graphs
    int col1_w = (int)((float)w * 0.6f);
    int col2_w = w - col1_w - 1;
    
    tui_rect_t r_cpu = {{(int16_t)x, (int16_t)y}, {(int16_t)col1_w, 8}};
    tui_graph_t* cpu_graph = tui_graph_create(r_cpu);
    tui_container_add(&panel->base, (tui_widget_t*)cpu_graph);
    if(out_graph) *out_graph = cpu_graph;

    tui_rect_t r_mem = {{(int16_t)((int16_t)x + (int16_t)col1_w + 1), (int16_t)y}, {(int16_t)col2_w, 8}};
    tui_gauge_t* mem_gauge = tui_gauge_create(r_mem, 100.0f);
    tui_gauge_set_label(mem_gauge, "RAM");
    tui_container_add(&panel->base, (tui_widget_t*)mem_gauge);
    if(out_gauge) *out_gauge = mem_gauge;
    
    // Bottom: Table
    tui_rect_t r_table = {{(int16_t)x, (int16_t)(y + 9)}, {(int16_t)w, (int16_t)(h - 9)}};
    tui_table_t* table = tui_table_create(r_table);
    tui_table_add_column(table, "Process", 15, 0);
    tui_table_add_column(table, "PID", 8, 0);
    tui_table_add_column(table, "Status", 10, 0);
    tui_table_add_column(table, "CPU", 8, 1.0f);
    tui_container_add(&panel->base, (tui_widget_t*)table);
    
    const char* r1[] = {"systemd", "1", "sleeping", "0.0%"};
    tui_table_add_row(table, r1);
    
    // Auto-add some rows
    on_add_process(NULL, table);
    on_add_process(NULL, table);

    // Button to add more
    tui_rect_t r_btn = {{(int16_t)((int16_t)x + (int16_t)col1_w + 1), (int16_t)(y + 5)}, {(int16_t)col2_w, 1}};
    tui_button_t* btn = tui_button_create("Add Process", r_btn);
    tui_button_set_click_callback(btn, on_add_process, table);
    tui_container_add(&panel->base, (tui_widget_t*)btn);

    // Quit Button
    tui_rect_t r_quit = {{(int16_t)((int16_t)x + (int16_t)col1_w + 1), (int16_t)(y + 7)}, {(int16_t)col2_w, 1}};
    tui_button_t* btn_quit = tui_button_create("Quit", r_quit);
    tui_button_set_click_callback(btn_quit, on_quit, NULL);
    tui_container_add(&panel->base, (tui_widget_t*)btn_quit);

    return (tui_widget_t*)panel;
}

static tui_widget_t* create_forms_tab(tui_rect_t bounds) {
    tui_panel_t* panel = tui_panel_create(bounds, "Controls & Forms");
    int x = bounds.position.x + 2;
    int y = bounds.position.y + 2;
    
    // Checkboxes
    tui_label_t* l1 = tui_label_create("Preferences:", (tui_point_t){(int16_t)x, (int16_t)y});
    tui_container_add(&panel->base, (tui_widget_t*)l1);
    y+=2;
    
    tui_checkbox_t* cb1 = tui_checkbox_create("Enable Autosave", (tui_rect_t){{(int16_t)x, (int16_t)y}, {20, 1}});
    tui_checkbox_set_toggle_callback(cb1, on_checkbox_toggle, NULL);
    tui_container_add(&panel->base, (tui_widget_t*)cb1);
    y++;
    
    tui_checkbox_t* cb2 = tui_checkbox_create("Dark Mode", (tui_rect_t){{(int16_t)x, (int16_t)y}, {20, 1}});
    cb2->checked = true;
    cb2->state = 1;
    tui_container_add(&panel->base, (tui_widget_t*)cb2);
    y+=3;
    
    // Radios
    tui_label_t* l2 = tui_label_create("Network Mode:", (tui_point_t){(int16_t)x, (int16_t)y});
    tui_container_add(&panel->base, (tui_widget_t*)l2);
    y+=2;
    
    tui_radio_button_t* rb1 = tui_radio_button_create("Offline", (tui_rect_t){{(int16_t)x, (int16_t)y}, {20, 1}}, 1);
    tui_container_add(&panel->base, (tui_widget_t*)rb1);
    y++;
    tui_radio_button_t* rb2 = tui_radio_button_create("Online", (tui_rect_t){{(int16_t)x, (int16_t)y}, {20, 1}}, 1);
    rb2->selected = true;
    tui_container_add(&panel->base, (tui_widget_t*)rb2);
    y+=3;
    
    // Input
    tui_label_t* l3 = tui_label_create("Username:", (tui_point_t){(int16_t)x, (int16_t)y});
    tui_container_add(&panel->base, (tui_widget_t*)l3);
    y+=2;
    tui_input_field_t* input = tui_input_field_create((tui_rect_t){{(int16_t)x, (int16_t)y}, {30, 1}}, 32);
    tui_container_add(&panel->base, (tui_widget_t*)input);
    
    return (tui_widget_t*)panel;
}

static tui_widget_t* create_list_tab(tui_rect_t bounds) {
    tui_panel_t* panel = tui_panel_create(bounds, "Logs & Events");
    int x = bounds.position.x + 1;
    int y = bounds.position.y + 1;
    int w = bounds.size.width - 2;
    int h = bounds.size.height - 2;
    
    tui_list_t* list = tui_list_create((tui_rect_t){{(int16_t)x, (int16_t)y}, {(int16_t)w, (int16_t)h}});
    for(int i=0; i<50; i++) {
        char buf[64];
        sprintf(buf, "[LOG %03d] System event occurred at timestamp %d", i, 160000 + i*100);
        tui_list_add_item(list, buf);
    }
    tui_container_add(&panel->base, (tui_widget_t*)list);
    return (tui_widget_t*)panel;
}

void app_tui_run_demo(void) {
    if (!tui_init()) return;

    tui_size_t screen = tui_get_screen_size();
    tui_rect_t bounds = {{0, 0}, screen};
    
    // Root is a Tab Container
    tui_tab_container_t* tabs = tui_tab_container_create(bounds);
    tui_set_root((tui_widget_t*)tabs);

    // Create Tabs
    tui_graph_t* cpu_graph = NULL;
    tui_gauge_t* mem_gauge = NULL;
    
    // Bounds for content (tab logic usually resizes, but we pass full bounds and let layout handle it roughly)
    // The tab_container_add_tab we implemented resizes the content.
    tui_rect_t content_bounds = bounds; // Placeholder

    tui_widget_t* dash = create_dashboard_tab(content_bounds, &cpu_graph, &mem_gauge);
    tui_tab_container_add_tab(tabs, "Dashboard", dash);
    
    tui_widget_t* forms = create_forms_tab(content_bounds);
    tui_tab_container_add_tab(tabs, "Controls", forms);
    
    tui_widget_t* lists = create_list_tab(content_bounds);
    tui_tab_container_add_tab(tabs, "Logs", lists);

    // Quit button floating? or in a tab?
    // Quit button is already on Dashboard
    (void)on_quit; (void)on_radio_select; // Mark used to suppress warning if we don't wire them up yet
    
    // Initial drawput a global Quit Key (q) or a button on Dashboard.
    // There is a quit button in create_dashboard_tab code (Wait, I removed it in creation above? No I added 'Add Process').
    // Let's add explicit Quit button to Dashboard.
    // ... Actually I didn't add the quit button in `create_dashboard_tab` above, let me check...
    // I see `on_quit` defined but not used.
    // I'll add a quit button to the Dashboard tab.
    
    // Initial draw
    tui_render();
    
    while (tui_is_running()) {
        tui_process_events();
        
        // Update Graph (only if dashboard is active? or always)
        if (cpu_graph) {
            static float t = 0;
            t += 0.1f;
            float val = 50.0f + 30.0f * sinf(t);
            tui_graph_add_value(cpu_graph, val);
            tui_gauge_set_value(mem_gauge, fabsf(100.0f * sinf(t * 0.5f)));
        }
        
        tui_render();
        tui_sleep(50);
    }
    
    tui_cleanup();
}
