#include "tui_registry.h"
#include <string.h>
#include <stdio.h>

/**
 * @file tui_registry.c
 * @brief Implementation of command and keybinding registry.
 */

// Static storage for simplicity (Global Registry)
static tui_command_t s_commands[TUI_MAX_COMMANDS];
static uint32_t s_command_count = 0;

static tui_keybinding_t s_bindings[TUI_MAX_BINDINGS];
static uint32_t s_binding_count = 0;

void tui_registry_register_command(const char* id, const char* desc, tui_command_cb cb, void* user_data) {
    if (s_command_count >= TUI_MAX_COMMANDS) return; // Full

    tui_command_t* cmd = &s_commands[s_command_count++];
    strncpy(cmd->id, id, sizeof(cmd->id) - 1);
    cmd->id[sizeof(cmd->id) - 1] = '\0'; // ensure null termination
    
    strncpy(cmd->description, desc, sizeof(cmd->description) - 1);
    cmd->description[sizeof(cmd->description) - 1] = '\0';

    cmd->callback = cb;
    cmd->user_data = user_data;
}

void tui_registry_register_binding(int key_code, bool ctrl, tui_binding_context_t context, const char* command_id) {
    if (s_binding_count >= TUI_MAX_BINDINGS) return;

    tui_keybinding_t* bind = &s_bindings[s_binding_count++];
    bind->key_code = key_code;
    bind->ctrl_modifier = ctrl;
    bind->context = context;
    strncpy(bind->command_id, command_id, sizeof(bind->command_id) - 1);
    bind->command_id[sizeof(bind->command_id) - 1] = '\0';
}

static tui_command_t* _find_command(const char* id) {
    for (uint32_t i = 0; i < s_command_count; i++) {
        if (strcmp(s_commands[i].id, id) == 0) {
            return &s_commands[i];
        }
    }
    return NULL;
}

bool tui_registry_process_input(tui_context_t* ctx, int key_code, tui_binding_context_t active_context) {
    // Naive linear search:
    // 1. Look for specific context match
    // 2. Look for global context match as fallback

    // Check Active Context First
    for (uint32_t i = 0; i < s_binding_count; i++) {
        tui_keybinding_t* b = &s_bindings[i];
        if (b->context == active_context && b->key_code == key_code) {
            // Found match in active context
            tui_command_t* cmd = _find_command(b->command_id);
            if (cmd && cmd->callback) {
                cmd->callback(ctx, cmd->user_data);
                return true;
            }
        }
    }

    // Check Global Context
    if (active_context != TUI_BINDING_context_global) {
        for (uint32_t i = 0; i < s_binding_count; i++) {
            tui_keybinding_t* b = &s_bindings[i];
            if (b->context == TUI_BINDING_context_global && b->key_code == key_code) {
                tui_command_t* cmd = _find_command(b->command_id);
                if (cmd && cmd->callback) {
                    cmd->callback(ctx, cmd->user_data);
                    return true;
                }
            }
        }
    }

    return false; // No binding found
}

const tui_keybinding_t* tui_registry_get_bindings(uint32_t* out_count) {
    if (out_count) *out_count = s_binding_count;
    return s_bindings;
}

const char* tui_registry_get_command_desc(const char* command_id) {
    tui_command_t* cmd = _find_command(command_id);
    return cmd ? cmd->description : "";
}
