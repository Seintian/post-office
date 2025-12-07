#include "app_tui.h"
#include <postoffice/tui/tui.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void on_demo_update(void* data) {
    struct {
        tui_graph_t* graph;
        tui_gauge_t* gauge;
    } *ctx = data;

    if (ctx && ctx->graph) {
        static float t = 0;
        t += 0.1f;
        float val = 50.0f + 30.0f * sinf(t);
        tui_graph_add_value(ctx->graph, val);
        if (ctx->gauge) {
            tui_gauge_set_value(ctx->gauge, fabsf(100.0f * sinf(t * 0.5f)));
        }
    }
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
    tui_rect_t content_bounds = bounds; 
    // Fix layout offset: TabContainer starts content at y+2 (header+separator) plus margin +1 for border
    content_bounds.position.x += 1;
    content_bounds.position.y += 3; // +1 border, +1 header text, +1 separator
    content_bounds.size.width -= 2;
    content_bounds.size.height -= 4; // -2 border Y, -2 header Y

    tui_widget_t* dash = create_dashboard_tab(content_bounds, &cpu_graph, &mem_gauge);
    tui_tab_container_add_tab(tabs, "Dashboard", dash);
    
    tui_widget_t* forms = create_forms_tab(content_bounds);
    tui_tab_container_add_tab(tabs, "Controls", forms);
    
    tui_widget_t* lists = create_list_tab(content_bounds);
    tui_tab_container_add_tab(tabs, "Logs", lists);

    // Quit button floating? or in a tab?
    // Quit button is already on Dashboard
    (void)on_quit; (void)on_radio_select; // Mark used to suppress warning if we don't wire them up yet
    
    // Initial draw
    tui_render();

    tui_set_target_fps(20);
    
    // Context for animation callback
    struct demo_context {
        tui_graph_t* graph;
        tui_gauge_t* gauge;
    } ctx = { cpu_graph, mem_gauge };

    tui_set_update_callback(on_demo_update, &ctx);
    tui_run();
    
    tui_cleanup();
}

// --- Simulation TUI ---

// Forward declarations
static tui_widget_t* create_director_screen(void);
static tui_widget_t* create_ticket_issuer_screen(void);
static tui_widget_t* create_users_manager_screen(void);
static tui_widget_t* create_worker_screen(void);
static tui_widget_t* create_user_screen(void);

// Context for simulation TUI
typedef struct {
    tui_widget_t* content_container;
    tui_input_field_t* input;
} sim_context_t;

static sim_context_t g_sim_ctx;

static void on_sidebar_select(tui_list_t* list, int index, void* data) {
    (void)data;
    if (!g_sim_ctx.content_container) return;

    // Clear current content
    tui_container_remove_all((tui_container_t*)g_sim_ctx.content_container);

    tui_widget_t* new_content = NULL;
    const char* item_text = tui_list_get_item(list, index);
    
    if (item_text) {
        if (strcmp(item_text, "Director") == 0) {
            new_content = create_director_screen();
        } else if (strcmp(item_text, "Ticket Issuer") == 0) {
            new_content = create_ticket_issuer_screen();
        } else if (strcmp(item_text, "Users Manager") == 0) {
            new_content = create_users_manager_screen();
        } else if (strcmp(item_text, "Worker") == 0) {
            new_content = create_worker_screen();
        } else if (strcmp(item_text, "User") == 0) {
            new_content = create_user_screen();
        }
    }

    if (new_content) {
        // Expand content to fill container
        new_content->layout_params.expand_x = true;
        new_content->layout_params.expand_y = true;
        new_content->layout_params.fill_x = true;
        new_content->layout_params.fill_y = true;
        tui_container_add((tui_container_t*)g_sim_ctx.content_container, new_content);
    }
    
    // Force layout update
    tui_widget_layout_update((tui_widget_t*)g_sim_ctx.content_container); // If available, or root update
}

static tui_widget_t* create_director_screen(void) {
    tui_rect_t bounds = {0};
    tui_tab_container_t* tabs = tui_tab_container_create(bounds);
    
    // Overview Tab
    tui_panel_t* p1 = tui_panel_create(bounds, NULL);
    tui_container_set_layout((tui_container_t*)p1, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1)); // Clean inner spacing
    // Add padding to panel content so text doesn't touch border
    tui_layout_params_set_padding(&p1->base.base.layout_params, 1, 1, 1, 1);
    
    // Status moved to header, so just show some content here
    tui_label_t* l1 = tui_label_create("Director Logs:", (tui_point_t){0,0});
    tui_container_add(&p1->base, (tui_widget_t*)l1);
    tui_tab_container_add_tab(tabs, "Overview", (tui_widget_t*)p1);

    // Processes Tab
    tui_panel_t* p2 = tui_panel_create(bounds, "Processes");
    tui_container_set_layout((tui_container_t*)p2, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&p2->base.base.layout_params, 1, 1, 1, 1);
    
    tui_list_t* list = tui_list_create(bounds);
    tui_list_add_item(list, "Process 1 (Running)");
    tui_list_add_item(list, "Process 2 (Idle)");
    list->base.layout_params.expand_y = true;
    list->base.layout_params.fill_x = true;
    list->base.layout_params.weight_y = 1.0f; 
    tui_container_add(&p2->base, (tui_widget_t*)list);
    tui_tab_container_add_tab(tabs, "Processes", (tui_widget_t*)p2);

    return (tui_widget_t*)tabs;
}

static tui_widget_t* create_ticket_issuer_screen(void) {
    tui_rect_t bounds = {0};
    tui_tab_container_t* tabs = tui_tab_container_create(bounds);
    
    tui_panel_t* p1 = tui_panel_create(bounds, NULL);
    tui_container_set_layout((tui_container_t*)p1, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&p1->base.base.layout_params, 1, 1, 1, 1);

    tui_label_t* l1 = tui_label_create("Issued Tickets: 12", (tui_point_t){0,0});
    tui_container_add(&p1->base, (tui_widget_t*)l1);
    tui_tab_container_add_tab(tabs, "Status", (tui_widget_t*)p1);

    return (tui_widget_t*)tabs;
}

static tui_widget_t* create_users_manager_screen(void) {
    tui_rect_t bounds = {0};
    tui_tab_container_t* tabs = tui_tab_container_create(bounds);
    
    tui_panel_t* p1 = tui_panel_create(bounds, NULL);
    tui_container_set_layout((tui_container_t*)p1, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&p1->base.base.layout_params, 1, 1, 1, 1);

    tui_label_t* l1 = tui_label_create("Active Users: 3", (tui_point_t){0,0});
    tui_container_add(&p1->base, (tui_widget_t*)l1);
    tui_tab_container_add_tab(tabs, "Users", (tui_widget_t*)p1);

    return (tui_widget_t*)tabs;
}

static tui_widget_t* create_worker_screen(void) {
    tui_rect_t bounds = {0};
    tui_tab_container_t* tabs = tui_tab_container_create(bounds);
    
    tui_panel_t* p1 = tui_panel_create(bounds, "Job Queue");
    tui_container_set_layout((tui_container_t*)p1, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&p1->base.base.layout_params, 1, 1, 1, 1);

    tui_label_t* l1 = tui_label_create("Active Jobs: 5", (tui_point_t){0,0});
    tui_container_add(&p1->base, (tui_widget_t*)l1);
    tui_tab_container_add_tab(tabs, "Queue", (tui_widget_t*)p1);

    return (tui_widget_t*)tabs;
}

static tui_widget_t* create_user_screen(void) {
    tui_rect_t bounds = {0};
    tui_tab_container_t* tabs = tui_tab_container_create(bounds);
    
    tui_panel_t* p1 = tui_panel_create(bounds, "Request Form");
    tui_container_set_layout((tui_container_t*)p1, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));
    tui_layout_params_set_padding(&p1->base.base.layout_params, 2, 1, 2, 1); // More horizontal padding

    tui_container_add(&p1->base, (tui_widget_t*)tui_label_create("Enter Request:", (tui_point_t){0,0}));
    
    tui_input_field_t* inp = tui_input_field_create(bounds, 50);
    inp->base.layout_params.fill_x = true;
    inp->base.layout_params.min_height = 1;
    // Add margin top to input
    tui_layout_params_set_margin(&inp->base.layout_params, 0, 1, 0, 0); 
    tui_container_add(&p1->base, (tui_widget_t*)inp);
    
    tui_tab_container_add_tab(tabs, "New Request", (tui_widget_t*)p1);

    return (tui_widget_t*)tabs;
}

void app_tui_run_simulation(void) {
    if (!tui_init()) return;

    // Use full screen
    tui_size_t screen = tui_get_screen_size();
    tui_rect_t bounds = {{0, 0}, screen};
    
    // Root container
    tui_container_t* root = tui_container_create();
    tui_widget_set_bounds((tui_widget_t*)root, bounds);
    // Vertical Box Layout for Root
    tui_container_set_layout(root, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0));
    // Root padding to avoid screen edge
    // tui_layout_params_set_padding(&root->base.layout_params, 1, 1, 1, 1); 
    tui_set_root((tui_widget_t*)root);

    // 1. Header
    tui_panel_t* header = tui_panel_create(bounds, NULL); 
    header->show_border = true;
    header->base.base.layout_params.min_height = 3;
    header->base.base.layout_params.fill_x = true;
    // Use center alignment for title
    tui_container_set_layout(&header->base, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0));
    tui_layout_params_set_padding(&header->base.base.layout_params, 0, 1, 0, 0); // Padding top 1 in header content
    
    tui_container_add(root, (tui_widget_t*)header);

    tui_label_t* title = tui_label_create("Post Office Simulation", (tui_point_t){0, 0});
    title->base.layout_params.h_align = TUI_ALIGN_CENTER; // Center the label
    title->base.layout_params.fill_x = true; 
    tui_container_add(&header->base, (tui_widget_t*)title);
    
    tui_label_t* subtitle = tui_label_create("Director Status: Running", (tui_point_t){0, 0});
    subtitle->base.layout_params.h_align = TUI_ALIGN_CENTER;
    subtitle->base.layout_params.fill_x = true;
    tui_container_add(&header->base, (tui_widget_t*)subtitle);
    
    // 2. Middle Container (Sidebar + Content)
    tui_container_t* middle = tui_container_create();
    middle->base.layout_params.weight_y = 1.0f; // Expand vertically
    middle->base.layout_params.fill_x = true;   // Fill width
    middle->base.layout_params.expand_y = true;
    // Add margin between header and middle?
    // tui_layout_params_set_margin(&middle->base.layout_params, 0, 1, 0, 1); 
    
    tui_container_set_layout(middle, tui_layout_box_create(TUI_ORIENTATION_HORIZONTAL, 0)); // 0 spacing, sidebar border will handle
    tui_container_add(root, (tui_widget_t*)middle);

    // 2a. Sidebar
    // Wrap List in a Panel to get a Border
    tui_panel_t* sidebar_panel = tui_panel_create(bounds, "Menu");
    sidebar_panel->base.base.layout_params.min_width = 25; // Wider sidebar
    sidebar_panel->base.base.layout_params.expand_y = true;
    sidebar_panel->base.base.layout_params.weight_x = 0; 
    
    tui_container_set_layout(&sidebar_panel->base, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0));
    tui_layout_params_set_padding(&sidebar_panel->base.base.layout_params, 1, 1, 1, 1); // Padding inside sidebar border

    tui_list_t* sidebar = tui_list_create(bounds);
    sidebar->base.layout_params.expand_y = true;
    sidebar->base.layout_params.fill_x = true;
    
    tui_list_add_item(sidebar, "Director");
    tui_list_add_item(sidebar, "Ticket Issuer");
    tui_list_add_item(sidebar, "Users Manager");
    tui_list_add_item(sidebar, "Worker");
    tui_list_add_item(sidebar, "User");
    tui_list_set_select_callback(sidebar, on_sidebar_select, NULL);
    
    tui_container_add(&sidebar_panel->base, (tui_widget_t*)sidebar);
    tui_container_add(middle, (tui_widget_t*)sidebar_panel);

    // 2b. Content Area
    tui_container_t* content = tui_container_create();
    content->base.layout_params.weight_x = 1.0f; 
    content->base.layout_params.expand_y = true; 
    content->base.layout_params.fill_x = true;
    // Add margin left to separate from sidebar
    tui_layout_params_set_margin(&content->base.layout_params, 1, 0, 0, 0);
    
    tui_container_set_layout(content, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0)); 
    tui_container_add(middle, (tui_widget_t*)content);
    
    g_sim_ctx.content_container = (tui_widget_t*)content;

    // 3. Command Input
    // Use a panel with title "Command"
    tui_panel_t* input_panel = tui_panel_create(bounds, "Command");
    input_panel->base.base.layout_params.min_height = 3;
    input_panel->base.base.layout_params.fill_x = true;
    
    tui_container_set_layout(&input_panel->base, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0));
    tui_container_set_layout(&input_panel->base, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 0));
    // Increase bottom padding to lift input from bottom edge or vice versa
    // User said "input is a row too high", so maybe we simply add padding top/bottom to center it or push it down
    tui_layout_params_set_padding(&input_panel->base.base.layout_params, 1, 0, 1, 0); // Padding left/right only for now


    tui_input_field_t* input = tui_input_field_create(bounds, 128);
    input->base.layout_params.fill_x = true;
    input->base.layout_params.min_height = 1;
    // Add margin top to push it down if needed, or margin bottom to lift it up?
    // "input is a row too high" -> It's too high up? Meaning it needs to go down.
    // If the panel is 3 high, and input is 1 high, and vertical box...
    // Adding top margin might push it down.
    tui_layout_params_set_margin(&input->base.layout_params, 1, 0, 0, 0);

    tui_container_add((tui_container_t*)input_panel, (tui_widget_t*)input);
    tui_container_add(root, (tui_widget_t*)input_panel);
    
    g_sim_ctx.input = input;

    // Trigger initial selection
    on_sidebar_select(sidebar, 0, NULL);
    
    // Force layout update before first render
    tui_widget_layout_update((tui_widget_t*)root);

    tui_render();
    tui_run();
    tui_cleanup();
}
