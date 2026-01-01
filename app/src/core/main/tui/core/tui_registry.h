#ifndef TUI_REGISTRY_H
#define TUI_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>
#include "tui_context.h"

/**
 * @file tui_registry.h
 * @brief Central registry for commands and keybindings.
 */

// Max limits for static allocation simplicity
#define TUI_MAX_COMMANDS 128
#define TUI_MAX_BINDINGS 256

/**
 * @brief Function pointer type for command callbacks.
 * 
 * @param ctx The active TUI context.
 * @param user_data Optional user data registered with the command.
 */
typedef void (*tui_command_cb)(tui_context_t* ctx, void* user_data);

/**
 * @brief Describes a single command executable by the system.
 */
typedef struct {
    char id[32];            // Unique string ID, e.g., "nav.down"
    char description[64];   // Human readable description
    tui_command_cb callback;
    void* user_data;
} tui_command_t;

/**
 * @brief Context in which a keybinding is valid.
 */
typedef enum {
    TUI_BINDING_context_global,
    TUI_BINDING_context_simulation,
    TUI_BINDING_context_config,
    TUI_BINDING_context_logs,
    TUI_BINDING_context_editor,
    TUI_BINDING_context_entities,
    // Add others as needed
    TUI_BINDING_CONTEXT_COUNT
} tui_binding_context_t;

/**
 * @brief Maps a key input to a command ID.
 */
typedef struct {
    int key_code;                    // Ncurses key code (e.g., 'q', KEY_UP)
    bool ctrl_modifier;              // Requires CTRL modifier?
    tui_binding_context_t context;   // Active context requirement
    char command_id[32];             // ID of command to trigger
} tui_keybinding_t;

/**
 * @brief Registers a command with the system.
 * 
 * @param id Unique ID string.
 * @param desc Description for Help screen.
 * @param cb Callback function.
 * @param user_data Optional data.
 */
void tui_registry_register_command(const char* id, const char* desc, tui_command_cb cb, void* user_data);

/**
 * @brief Binds a key to a command ID within a specific context.
 * 
 * @param key_code Key to bind.
 * @param ctrl Is CTRL required?
 * @param context Context where this binding applies.
 * @param command_id Target command ID.
 */
void tui_registry_register_binding(int key_code, bool ctrl, tui_binding_context_t context, const char* command_id);

/**
 * @brief Processes a key input and triggers the matching command if found.
 * 
 * @param ctx Active TUI context.
 * @param key_code Input key code.
 * @param active_context Current active UI context (usually derived from screen).
 * @return true if a command was executed, false otherwise.
 */
bool tui_registry_process_input(tui_context_t* ctx, int key_code, tui_binding_context_t active_context);

/**
 * @brief Retrieves all registered bindings (useful for Help Screen).
 * 
 * @param out_count output pointer for count.
 * @return Pointer to internal array of bindings.
 */
const tui_keybinding_t* tui_registry_get_bindings(uint32_t* out_count);

/**
 * @brief Retrieves command description by ID.
 */
const char* tui_registry_get_command_desc(const char* command_id);

#endif // TUI_REGISTRY_H
