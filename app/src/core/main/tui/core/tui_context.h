#ifndef TUI_CONTEXT_H
#define TUI_CONTEXT_H

#ifndef CLAY_IMPLEMENTATION
#include <clay/clay.h>
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @file tui_context.h
 * @brief Defines the core TUI context and state management.
 * @author Post-Office Team
 *
 * This file contains the definition of the tui_context_t structure, which
 * holds the global state of the TUI, including navigation, selected items,
 * and system statistics. It replaces the legacy tuiState global.
 */

// Forward declarations
struct tui_registry_s;

/**
 * @brief Enumeration of available main screens.
 */
typedef enum {
    TUI_SCREEN_SIMULATION,
    TUI_SCREEN_PERFORMANCE,
    TUI_SCREEN_LOGS,
    TUI_SCREEN_CONFIG,
    TUI_SCREEN_ENTITIES,
    TUI_SCREEN_NETWORK,
    TUI_SCREEN_HELP,
    TUI_SCREEN_DIRECTOR,
    TUI_SCREEN_COUNT
} tui_screen_t;

/**
 * @brief Main Context structure for the TUI.
 *
 * Holds the runtime state of the TUI application.
 */
typedef struct {
    // Navigation
    tui_screen_t current_screen;
    tui_screen_t previous_screen; // for "Go Back" functionality if needed

    // Stats (Cached)
    float fps;
    float cpu_usage;
    float mem_usage_mb;

    // State flags
    bool is_running;
    bool show_error;
    char error_message[256];

    // Input State
    char input_buffer[256];
    uint32_t input_cursor;

    // Registry reference (if needed, or kept separate)
    // struct tui_registry_s *registry;

    // Clay Arena reference (handled by main mostly, but good to track)
    Clay_Arena arena;

} tui_context_t;

/**
 * @brief Creates and initializes a new TUI context.
 * 
 * @param arena_size Size of the memory arena to allocate for Clay.
 * @return Pointer to the new context, or NULL on failure.
 */
tui_context_t* tui_context_create(size_t arena_size);

/**
 * @brief Destroys the TUI context and frees resources.
 * 
 * @param ctx Pointer to the context to destroy.
 */
void tui_context_destroy(tui_context_t* ctx);

/**
 * @brief Resets transient per-frame state in the context.
 * 
 * Should be called at the start of each render frame.
 * 
 * @param ctx Pointer to the context.
 */
void tui_context_update(tui_context_t* ctx);

#endif // TUI_CONTEXT_H
