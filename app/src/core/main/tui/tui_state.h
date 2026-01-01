#ifndef TUI_STATE_H
#define TUI_STATE_H

#ifndef CLAY_IMPLEMENTATION
#include <clay/clay.h>
#endif

#include <stdbool.h>
#include <string.h>

#include "utils/configs.h"
#include "components/data_table.h" // Added for DataTableState

/**
 * @brief Header for shared TUI state, constants, and helper macros.
 * 
 * This file serves as the central definition for the application's TUI state,
 * allow components and screens to access shared data like the current screen,
 * input buffer, and system statistics.
 */

// --- Constants ---
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
#define CTRL_KEY(k) ((k) & 0x1f)

// --- Enums ---

/**
 * @brief Enumeration of available main screens in the TUI.
 */
typedef enum {
    SCREEN_SIMULATION,  // Dashboard / Simulation Overview
    SCREEN_PERFORMANCE, // Detailed Performance Metrics
    SCREEN_LOGS,        // System Logs
    SCREEN_CONFIG,      // Configuration Editor
    SCREEN_ENTITIES,    // Entities Table
    SCREEN_NETWORK,     // Network / IPC Topology
    SCREEN_HELP,        // Help & Shortcuts
    SCREEN_DIRECTOR_CTRL, // Manual Director Controls
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

typedef struct {
    char section[64];
    char key[64];
    char displayKey[128]; // Pre-formatted key for display "Section.Key"
    char value[256];
} ConfigItem;

// --- Mock Data Structures ---

typedef enum {
    ENTITY_TYPE_DIRECTOR,
    ENTITY_TYPE_MANAGER, // Issuer, UserMgr
    ENTITY_TYPE_WORKER,
    ENTITY_TYPE_USER
} EntityType;

typedef struct {
    uint32_t id;
    EntityType type;
    char name[32];
    char state[32]; // "Idle", "Working", "Queue"
    char location[32]; // "Pool", "Lobby", "Counter"
    char currentTask[64];
    float cpuUsage;
    int memoryUsageMB;
    int queueDepth;
} MockEntity;

#define MAX_MOCK_ENTITIES 200

// --- Mock IPC Data ---
typedef struct {
    char name[32];
    char type[32]; // "Director", "Issuer", "Worker"
    bool active;
    Clay_Vector2 position; // Visual position for topology
} MockIPCNode;

typedef struct {
    int fromNodeIndex;
    int toNodeIndex;
    int messagesPerSec;
    int bandwidthBytesPerSec;
    uint32_t bufferUsagePercent;
} MockIPCChannel;

#define MAX_MOCK_NODES 16
#define MAX_MOCK_CHANNELS 32

// --- Help Screen Data ---
typedef struct {
    char key[32];
    char description[128];
    char context[32]; // "Global", "Simulation", etc.
} Keybinding;

#define MAX_HELP_BINDINGS 32

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
    uint32_t activeLogTab;     // Active tab index for Logs Screen
    DataTableState logTableState; // Internal for some screen log views if needed?

    uint32_t logFileCount;
    char logFiles[16][64];     // Max 16 files, 64 chars each
    Clay_Vector2 logScrollPosition;
    long logReadOffset;        // File byte offset for "Top of View"

    // Entities Screen
    MockEntity mockEntities[MAX_MOCK_ENTITIES];
    uint32_t mockEntityCount;
    DataTableState entitiesTableState;
    int selectedEntityIndex;   // Index in mockEntities, -1 if none
    uint32_t activeEntitiesTab; // 0=All/System, 1=Simulation
    char entitiesFilter[64];
    bool isFilteringEntities;
    uint32_t filteredEntityCount;
    int filteredEntityIndices[MAX_MOCK_ENTITIES];

    // IPC Screen
    MockIPCNode mockIPCNodes[MAX_MOCK_NODES];
    uint32_t mockIPCNodeCount;
    MockIPCChannel mockIPCChannels[MAX_MOCK_CHANNELS];
    uint32_t mockIPCChannelCount;
    DataTableState ipcTableState;

    // Help Screen
    Keybinding helpBindings[MAX_HELP_BINDINGS];
    uint32_t helpBindingCount;
    DataTableState helpTableState;

    // Director Control Screen
    bool simIsRunning;
    char currentScenario[64];
    uint32_t activeWorkers;
    uint32_t activeUsers;

    // Configuration State (Editor)
    char configFiles[16][64];    // Available config files (Tabs)
    uint32_t configFileCount;
    uint32_t activeConfigTab;    // Index of active tab

    // Editor Interaction
    po_config_t *loadedConfigHandle; // Opaque handle to current config

    // Display Cache (Flattened list for scrolling/selection)
    ConfigItem configDisplayItems[128]; 
    uint32_t configDisplayCount;
    uint32_t selectedConfigItemIndex;
    uint32_t lastSelectedConfigItemIndex; // Detect change for auto-scroll
    uint32_t hoveredConfigItemIndex; // For mouse hover highlight
    uint32_t maxKeyLength; // For dynamic column sizing
    float configScrollY;

    // Editing
    bool isEditing;
    char editValueBuffer[256];
    char editInputDisplay[260]; // Persistent buffer for edit field display "Value_"

    // Status
    char lastSavedMessage[64]; // "Saved to disk" feedback
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
// Usage: CLAY_ID_IDX2("MyItem", row, col)
#define CLAY_ID_IDX(label, idx) CLAY_IDI(label, idx)
#define CLAY_ID_IDX2(label, r, c) CLAY_IDI(label, (r) * 1000 + (c))

// --- Helper Functions ---
/**
 * @brief Returns a pointer to a formatted string stored in a static ring buffer.
 * Valid until the buffer wraps around (typically safe for a single frame of UI).
 */
char* tui_ScratchFmt(const char* fmt, ...);
char* tui_ScratchAlloc(size_t size);
void tui_ResetScratch(void);

/**
 * @brief Singleton instance of the TUI state.
 * Defined in app_tui.c.
 */
extern tuiState g_tuiState;

#endif // TUI_STATE_H
