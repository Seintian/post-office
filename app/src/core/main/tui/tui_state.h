#ifndef TUI_STATE_H
#define TUI_STATE_H

#ifndef CLAY_IMPLEMENTATION
#include <clay/clay.h>
#endif

#include <stdbool.h>
#include <string.h>

/**
 * @brief Header for shared TUI state, constants, and helper macros.
 * 
 * This file serves as the central definition for the application's TUI state,
 * allow components and screens to access shared data like the current screen,
 * input buffer, and system statistics.
 */

// --- Constants ---
#define TUI_CW 8   // Cell Width in pixels (approximate, for layout calculations)
#define TUI_CH 16  // Cell Height in pixels

// Common color definitions
#define COLOR_ACCENT (Clay_Color) { 100, 200, 255, 255 }
#define COLOR_ERROR (Clay_Color) { 255, 100, 100, 255 }
#define COLOR_TEXT_DIM (Clay_Color) { 120, 120, 120, 255 }

// Helper to create a Clay_String from a dynamic C string (pointer).
// Note: Ensure the string usage does not outlive the pointer validity.
#define CLAY_STRING_DYN(s) (Clay_String) { .length = (int32_t)strlen(s), .chars = (s) }

#define INPUT_BUFFER_SIZE 256

// --- Enums ---

/**
 * @brief Enumeration of available main screens in the TUI.
 */
typedef enum {
    SCREEN_SIMULATION,  // Dashboard / Simulation Overview
    SCREEN_PERFORMANCE, // Detailed Performance Metrics
    SCREEN_COUNT
} tuiScreen;

/**
 * @brief Tabs available within the Simulation Screen.
 */
typedef enum {
    TAB_SIM_DIRECTOR,
    TAB_SIM_USERS,
    TAB_SIM_COUNT
} SimTab;

/**
 * @brief Tabs available within the Performance Screen.
 */
typedef enum {
    TAB_PERF_SYSTEM,
    TAB_PERF_LIBRARIES,
    TAB_PERF_STATS,
    TAB_PERF_COUNT
} PerfTab;

// --- State Struct ---

/**
 * @brief Global TUI State structure.
 * 
 * Holds all mutable state for the Text User Interface, including navigation,
 * input buffers, and cached system statistics.
 */
typedef struct {
    // Navigation State
    tuiScreen currentScreen;   // Currently active main screen
    uint32_t activeSimTab;     // Active tab index for Simulation Screen
    uint32_t activePerfTab;    // Active tab index for Performance Screen

    // Input State
    char inputBuffer[INPUT_BUFFER_SIZE]; // Buffer for command input (footer)
    uint32_t inputCursor;                // Current cursor position in inputBuffer

    // System Stats (Mocked for now)
    float fps;
    float cpuUsage;
    float memUsage; // In MB

    // Control
    bool running;          // Main loop flag. Set to false to exit.
    bool showError;        // If true, an error overlay is rendered.
    char errorMessage[256];
} tuiState;

// --- ID Generation Helper ---
// Simple index-based ID generation for dynamic lists (wrapper around Clay's IDI macros)
// Usage: CLAY_ID_IDX("MyItem", index)
#define CLAY_ID_IDX(label, idx) CLAY_IDI(label, idx)

/**
 * @brief Singleton instance of the TUI state.
 * Defined in app_tui.c.
 */
extern tuiState g_tuiState;

#endif // TUI_STATE_H
