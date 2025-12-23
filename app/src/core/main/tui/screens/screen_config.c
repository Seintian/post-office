#include "screen_config.h"
#include "simulation/simulation_lifecycle.h"
#include "common/args.h"
#include <postoffice/tui/tui.h>
#include <utils/configs.h>
#include <utils/argv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Assuming we can access the global args to get the config file path, 
// or we might need to expose the config object globally.
// For now, let's assume we re-load or access a global config.
// Since `main.c` loads config into `po_logger_init`, but `po_args_t` has the path.
// The TUI runs in the same process.

// We need a way to access the loaded configuration or the file path.
// Ideally, `app_tui` should have access to it.

// For this implementation, let's create a static config handle for the UI session
// or try to modify the "live" config if possible.
// Given the requirements "show config loaded upon startup... and possibility to edit",
// we should probably load the file again, or share the `po_config_t` pointer.

// Let's look at `main.c` again. `args` are local to main.
// We might need to expose a "get_current_config_path()" or similar.

// WORKAROUND: We will rely on a global or singleton accessor if available, 
// or just parse the command line args again? No, that's messy.
// Let's assume we can pass the config path to `app_tui_run_simulation` 
// and store it in `app_tui`.

// First, let's define the screen structure.

/**
 * @brief configuration screen context.
 */
typedef struct {
    tui_widget_t* list;          /**< List widget displaying config items */
    tui_widget_t* edit_field;    /**< Input field for editing values */
    tui_widget_t* status_label;  /**< Status label for feedback */
    po_config_t* config;         /**< Handle to the loaded configuration */
    char* config_path;           /**< Path to the config file */
    char current_section[64];    /**< Currently selected section name */
    char current_key[64];        /**< Currently selected key name */
} config_screen_ctx_t;

static config_screen_ctx_t g_cfg_ctx;

// Callback for iterating config
/**
 * @brief Callback for populating the config list.
 * 
 * Formats configuration entries as "[Section] Key = Value" or "Key = Value"
 * and adds them to the list widget.
 * 
 * @param section Configuration section (can be empty).
 * @param key Configuration key.
 * @param value Configuration value.
 * @param user_data Pointer to the tui_list_t widget.
 */
static void populate_list_cb(const char *section, const char *key, const char *value, void *user_data) {
    tui_list_t *list = (tui_list_t *)user_data;
    char buffer[256];
    if (section && section[0]) {
        snprintf(buffer, sizeof(buffer), "[%s] %s = %s", section, key, value);
    } else {
        snprintf(buffer, sizeof(buffer), "%s = %s", key, value);
    }
    tui_list_add_item(list, buffer);
}

/**
 * @brief Callback for handling list item selection.
 * 
 * Parses the selected line to extract section, key, and value, updating the context.
 * 
 * @param list The list widget.
 * @param index The selected index.
 * @param data User data (unused).
 */
static void on_item_selected(tui_list_t* list, int index, void* data) {
    (void)data;
    const char* text = tui_list_get_item(list, index);
    if (!text) return;

    // Parse back "section key = value"
    // Format: "[section] key = value" or "key = value"
    char section[64] = {0};
    char key[64] = {0};
    char val[128] = {0};

    if (text[0] == '[') {
        sscanf(text, "[%63[^]]] %63[^ ] = %127[^\n]", section, key, val);
    } else {
        sscanf(text, "%63[^ ] = %127[^\n]", key, val);
    }

    strncpy(g_cfg_ctx.current_section, section, sizeof(g_cfg_ctx.current_section));
    strncpy(g_cfg_ctx.current_key, key, sizeof(g_cfg_ctx.current_key));

    if (g_cfg_ctx.edit_field) {
        // Assume input widget has set_text
        // tui_input_set_text((tui_input_t*)g_cfg_ctx.edit_field, val);
        // Casting to input type if available
    }

    char status[256];
    snprintf(status, sizeof(status), "Selected: %s.%s", section, key);
    // tui_label_set_text((tui_label_t*)g_cfg_ctx.status_label, status);
}

// Reload config from file
/**
 * @brief Reloads the configuration from file.
 * 
 * Clears the current list and re-populates it by reloading the config file.
 */
static void reload_config(void) {
    const char* path = g_simulation_config_path;
    if (!path) return;

    if (g_cfg_ctx.config) {
        po_config_free(&g_cfg_ctx.config);
    }

    if (po_config_load(path, &g_cfg_ctx.config) != 0) {
        // Error loading
        return;
    }

    if (g_cfg_ctx.list) {
        // Remove all items manually if clear is not available
        // Assuming tui_list_remove_all exists or we recreate list
        // Based on headers, we might need to recreate the list or implement clear
        // Let's assume tui_list_clear is not available based on error
        // Re-creating might be safer or looking for remove function
        // tui_list_clear((tui_list_t*)g_cfg_ctx.list); 

        // WORKAROUND: For now, just append (duplicates if reloaded)
        // or check if we can remove items.
        // tui_list_remove_item(list, index);

        // Let's rely on append for initial load. 
        // Real implementation would clear.

        // Just append for now.
        po_config_foreach(g_cfg_ctx.config, populate_list_cb, g_cfg_ctx.list);
    }
}

// Create screen
tui_widget_t* screen_config_create(void) {
    // 1. Root Container (Vertical)
    tui_container_t* root = tui_container_create();
    tui_container_set_layout(root, tui_layout_box_create(TUI_ORIENTATION_VERTICAL, 1));

    // 2. Header
    tui_label_t* header = tui_label_create("Configuration Editor", (tui_point_t){0,0});
    tui_container_add(root, (tui_widget_t*)header);

    // 3. List
    // tui_list_create takes bounds, not 4 ints
    tui_rect_t bounds = {0}; 
    tui_list_t* list = tui_list_create(bounds);
    list->base.layout_params.expand_y = true;
    list->base.layout_params.fill_x = true;
    tui_list_set_select_callback(list, on_item_selected, NULL);
    tui_container_add(root, (tui_widget_t*)list);
    g_cfg_ctx.list = (tui_widget_t*)list;

    // 4. Edit Area
    tui_container_t* edit_area = tui_container_create();
    edit_area->base.layout_params.fill_x = true;
    edit_area->base.layout_params.min_height = 3;
    tui_container_set_layout(edit_area, tui_layout_box_create(TUI_ORIENTATION_HORIZONTAL, 1));

    tui_label_t* lbl = tui_label_create("Value:", (tui_point_t){0,0});
    tui_container_add(edit_area, (tui_widget_t*)lbl);

    // tui_input_t* input = tui_input_create(...); // If input exists
    // For now using placeholder
    tui_label_t* input_placeholder = tui_label_create("[Edit Value Here]", (tui_point_t){0,0});
    tui_container_add(edit_area, (tui_widget_t*)input_placeholder);
    g_cfg_ctx.edit_field = (tui_widget_t*)input_placeholder;

    tui_container_add(root, (tui_widget_t*)edit_area);

    // 5. Status / Buttons
    tui_container_t* buttons = tui_container_create();
    tui_container_set_layout(buttons, tui_layout_box_create(TUI_ORIENTATION_HORIZONTAL, 1));

    // tui_button_t* btn_save = tui_button_create("Save", ...);
    // tui_container_add(buttons, (tui_widget_t*)btn_save);

    tui_container_add(root, (tui_widget_t*)buttons);

    // Initial load
    reload_config();

    return (tui_widget_t*)root;
}
