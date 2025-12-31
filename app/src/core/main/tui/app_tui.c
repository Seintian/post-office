#ifndef CLAY_IMPLEMENTATION
#include <clay/clay.h>
#endif

#include <renderer/clay_ncurses_renderer.h>
#include <utils/signals.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "app_tui.h"
#include "tui_state.h"
#include "components/topbar.h"
#include "components/bottombar.h"
#include "screens/screen_dashboard.h"
#include "screens/screen_performance.h"
#include "screens/screen_logs.h"
#include "screens/screen_config.h"

// --- Macros ---
#define CTRL_KEY(k) ((k) & 0x1f)

// --- Internal Prototypes ---

static void tui_Initialize(void);
static void tui_Terminate(void);
static int tui_ProcessInput(void);
static void tui_Update(void);
static void tui_Render(void);

static void tui_RenderSidebar(void);
static void HandleKeyInput(int key);
static void OnSidebarClick(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData);


static int tui_OnSuspend(void);
static void tui_OnQuit(void);
static int tui_OnInterrupt(void);
static int tui_OnCrash(void);

// --- Layouts ---
static void RenderContent(void);
static void RenderErrorOverlay(void);

// --- Global State ---
/**
 * @brief Global TUI state instance, accessed by submodules for shared data.
 */
tuiState g_tuiState = {
    .currentScreen = SCREEN_SIMULATION,
    .activeSimTab = 0,
    .activePerfTab = 0,
    .inputBuffer = {0},
    .inputCursor = 0,
    .fps = 0.0f,
    .cpuUsage = 0.0f,
    .memUsage = 0.0f,
    .running = true,
    .showError = false,
    .errorMessage = {0}
};

// --- Core Lifecycle ---

/**
 * @brief Runs the TUI in demo/simulation mode.
 * 
 * Initializes the TUI, enters the main loop, and cleans up on exit.
 * 
 * @return 0 on success, non-zero on error.
 */
int app_tui_run_demo(void) {
    tui_Initialize();

    int result = 0;
    while (g_tuiState.running) {
        if (tui_ProcessInput() != 0) {
            result = 1;
            break;
        }
        if (!g_tuiState.running) break;

        tui_Update();
        tui_Render();

        usleep(33000); // ~30 FPS
    }

    tui_Terminate();
    return result;
}

int app_tui_run_simulation(void) {
    return app_tui_run_demo(); // Same loop for now
}

/**
 * @brief Initializes Clay and the Ncurses renderer.
 * Sets up memory arena and raw mode.
 */
static void tui_Initialize(void) {
    uint64_t totalMemorySize = 16 * 1024 * 1024; // 16 MB
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
    Clay_Initialize(arena, (Clay_Dimensions){0, 0}, (Clay_ErrorHandler){0});
    Clay_SetMeasureTextFunction(Clay_Ncurses_MeasureText, NULL);

    Clay_Ncurses_Initialize();
    Clay_Ncurses_SetRawMode(true); // capture all keys

    tui_InitLogsScreen();
    tui_InitConfigScreen();
}


/**
 * @brief Cleans up Clay and Ncurses resources.
 */
static void tui_Terminate(void) {
    Clay_Ncurses_Terminate();
}

// --- Input & Signal Handling ---

/**
 * @brief Processes all pending input events.
 * 
 * Drains the input queue from the Ncurses renderer to ensure low latency.
 * Handles system signals (Quit, Interrupt, Suspend) and passes other keys
 * to HandleKeyInput.
 * 
 * @return 0 on success, -1 on signal handling error.
 */
static int tui_ProcessInput(void) {
    while (true) {
        int key = Clay_Ncurses_ProcessInputStandard();
        if (key == -1 || key == ERR) break; // No more input

        // Check for Signal Keys first
        if (key == CLAY_NCURSES_KEY_SCROLL_UP) {
            if (g_tuiState.currentScreen == SCREEN_LOGS) {
                // Scroll "Up" means go back in file
                g_tuiState.logReadOffset -= 256; 
                if (g_tuiState.logReadOffset < 0) g_tuiState.logReadOffset = 0;
            } else if (g_tuiState.currentScreen == SCREEN_CONFIG) {
                g_tuiState.configScrollY -= 50.0f;
                if (g_tuiState.configScrollY < 0) g_tuiState.configScrollY = 0;
            } else {
                Clay_UpdateScrollContainers(true, (Clay_Vector2){0, -50}, 0.1f);
            }
        } else if (key == CLAY_NCURSES_KEY_SCROLL_DOWN) {
            if (g_tuiState.currentScreen == SCREEN_LOGS) {
                // Scroll "Down" means go forward in file
                g_tuiState.logReadOffset += 256;
                // Clamping to filesize happens in render view as we don't know filesize here easily
            } else if (g_tuiState.currentScreen == SCREEN_CONFIG) {
                 g_tuiState.configScrollY += 50.0f;
            } else {
                Clay_UpdateScrollContainers(true, (Clay_Vector2){0, 50}, 0.1f);
            }
        } else if (key == CLAY_NCURSES_KEY_SCROLL_LEFT) {
             if (g_tuiState.currentScreen == SCREEN_LOGS) {
                // Horizontal is visual shift
                g_tuiState.logScrollPosition.x += 50;
                if (g_tuiState.logScrollPosition.x > 0) g_tuiState.logScrollPosition.x = 0;
            } else {
                Clay_UpdateScrollContainers(true, (Clay_Vector2){-50, 0}, 0.1f);
            }
        } else if (key == CLAY_NCURSES_KEY_SCROLL_RIGHT) {
             if (g_tuiState.currentScreen == SCREEN_LOGS) {
                g_tuiState.logScrollPosition.x -= 50;
            } else {
                Clay_UpdateScrollContainers(true, (Clay_Vector2){50, 0}, 0.1f);
            }

        } else if (key == CTRL_KEY('q') || key == CTRL_KEY('Q')) {
            tui_OnQuit();
        } else if (key == CTRL_KEY('c') || key == CTRL_KEY('C')) {
            return tui_OnInterrupt();
        } else if (key == CTRL_KEY('z') || key == CTRL_KEY('Z')) {
            return tui_OnSuspend();
        } else if (key == CTRL_KEY('\\')) {
            return tui_OnCrash();
        } else {
            HandleKeyInput(key);
        }
    }
    return 0;
}

static void tui_OnQuit(void) {
    g_tuiState.running = false;
}

static int tui_OnInterrupt(void) {
    Clay_Ncurses_Terminate();
    if (sigutil_restore(SIGINT) != 0) {
        return -1;
    }
    raise(SIGINT);
    return 0;
}

static int tui_OnCrash(void) {
    Clay_Ncurses_Terminate();
    if (sigutil_restore(SIGQUIT) != 0) {
        return -1;
    }
    raise(SIGQUIT);
    return 0;
}

/**
 * @brief Handles process suspension (Ctrl+Z).
 * 
 * Temporarily restores terminal settings, suspends the process, 
 * and restores TUI mode upon resumption.
 */
static int tui_OnSuspend(void) {
    // 1. Lightweight Teardown
    Clay_Ncurses_PrepareSuspend();

    // 2. Prepare Signal (Unblock & Restore Default)
    if (sigutil_unblock(SIGTSTP) != 0) return -1;
    if (sigutil_restore(SIGTSTP) != 0) return -1;

    // 3. Suspend Process Group
    kill(0, SIGTSTP);

    // 4. Resume (Code continues here after 'fg')
    Clay_Ncurses_ResumeAfterSuspend();
    return 0;
}

/**
 * @brief Handles standard key inputs for navigation and text entry.
 * 
 * @param key The processed key code.
 */
static bool HandleConfigInput(int key);

/**
 * @brief Handles standard key inputs for navigation and text entry.
 * 
 * @param key The processed key code.
 */
static void HandleKeyInput(int key) {
    if (key == ERR) return;

    if (key == CTRL_KEY('s') || key == CTRL_KEY('S')) {
        if (g_tuiState.currentScreen == SCREEN_CONFIG) {
            tui_SaveCurrentConfig();
        }
        return;
    }

    if (g_tuiState.currentScreen == SCREEN_CONFIG) {
        if (HandleConfigInput(key)) return;
        // Fallthrough if not consumed (e.g. typing for bottom bar)
    }

    // Main Tab Navigation
    if (key == KEY_DOWN) {
        g_tuiState.currentScreen = (g_tuiState.currentScreen + 1) % SCREEN_COUNT;
        return;
    }
    if (key == KEY_UP) {
        g_tuiState.currentScreen = (g_tuiState.currentScreen - 1 + SCREEN_COUNT) % SCREEN_COUNT;
        return;
    }
    if (key == KEY_RIGHT) {
        if (g_tuiState.currentScreen == SCREEN_SIMULATION) {
            g_tuiState.activeSimTab = (g_tuiState.activeSimTab + 1) % TAB_SIM_COUNT;
        } else if (g_tuiState.currentScreen == SCREEN_PERFORMANCE) {
            g_tuiState.activePerfTab = (g_tuiState.activePerfTab + 1) % TAB_PERF_COUNT;
        } else if (g_tuiState.currentScreen == SCREEN_LOGS && g_tuiState.logFileCount > 0) {
            g_tuiState.activeLogTab = (g_tuiState.activeLogTab + 1) % g_tuiState.logFileCount;
            g_tuiState.logScrollPosition = (Clay_Vector2){0,0};
            g_tuiState.logReadOffset = -1; // Reset to tail
        }
        return;
    }
    if (key == KEY_LEFT) {
        if (g_tuiState.currentScreen == SCREEN_SIMULATION) {
            g_tuiState.activeSimTab = (g_tuiState.activeSimTab - 1 + TAB_SIM_COUNT) % TAB_SIM_COUNT;
        } else if (g_tuiState.currentScreen == SCREEN_PERFORMANCE) {
            g_tuiState.activePerfTab = (g_tuiState.activePerfTab - 1 + TAB_PERF_COUNT) % TAB_PERF_COUNT;
        } else if (g_tuiState.currentScreen == SCREEN_LOGS && g_tuiState.logFileCount > 0) {
            g_tuiState.activeLogTab = (g_tuiState.activeLogTab - 1 + g_tuiState.logFileCount) % g_tuiState.logFileCount;
            g_tuiState.logScrollPosition = (Clay_Vector2){0,0};
            g_tuiState.logReadOffset = -1; // Reset to tail
        }
        return;
    }

    // Horizontal Scrolling via Keyboard (Shift+Arrows)
    if (key == KEY_SLEFT) {
        if (g_tuiState.currentScreen == SCREEN_LOGS) {
            g_tuiState.logScrollPosition.x += 50;
            if (g_tuiState.logScrollPosition.x > 0) g_tuiState.logScrollPosition.x = 0;
        }
        return;
    }
    if (key == KEY_SRIGHT) {
        if (g_tuiState.currentScreen == SCREEN_LOGS) {
            g_tuiState.logScrollPosition.x -= 50;
        }
        return;
    }

    // Text Input Command Dispatch
    if (key == '\n' || key == KEY_ENTER) {
        if (g_tuiState.inputCursor > 0) {
            g_tuiState.inputCursor = 0;
            g_tuiState.inputBuffer[0] = '\0';
        }
    } else if (key == KEY_PPAGE) { // Page Up
        if (g_tuiState.currentScreen == SCREEN_LOGS) {
            g_tuiState.logReadOffset = 0; // Jump to start
        }
    } else if (key == KEY_NPAGE) { // Page Down
        if (g_tuiState.currentScreen == SCREEN_LOGS) {
            g_tuiState.logReadOffset = -1; // Jump to end
        }
    } else if (key == KEY_BACKSPACE || key == 127) {

        if (g_tuiState.inputCursor > 0) {
            g_tuiState.inputCursor--;
            g_tuiState.inputBuffer[g_tuiState.inputCursor] = '\0';
        }
    } else if (key >= 32 && key <= 126) {
        if (g_tuiState.inputCursor < INPUT_BUFFER_SIZE - 1) {
            g_tuiState.inputBuffer[g_tuiState.inputCursor++] = (char)key;
            g_tuiState.inputBuffer[g_tuiState.inputCursor] = '\0';
        }
    }
}

extern void tui_ConfigNextTab(void);
extern void tui_ConfigPrevTab(void);
extern void tui_ConfigEnterEdit(void);
extern void tui_ConfigCommitEdit(void);
extern void tui_ConfigCancelEdit(void);
extern void tui_ConfigAppendChar(char c);
extern void tui_ConfigBackspace(void);

static bool HandleConfigInput(int key) {
    if (g_tuiState.isEditing) {
        if (key == '\n' || key == KEY_ENTER) {
            tui_ConfigCommitEdit();
            return true;
        } else if (key == 27) { // ESC
            tui_ConfigCancelEdit();
            return true;
        } else if (key == KEY_BACKSPACE || key == 127) {
            tui_ConfigBackspace();
            return true;
        } else if (key >= 32 && key <= 126) {
            tui_ConfigAppendChar((char)key);
            return true;
        }
    } else {
        if (key == KEY_UP) {
            if (g_tuiState.selectedConfigItemIndex > 0) g_tuiState.selectedConfigItemIndex--;
            return true;
        } else if (key == KEY_DOWN) {
            if (g_tuiState.selectedConfigItemIndex < g_tuiState.configDisplayCount - 1) g_tuiState.selectedConfigItemIndex++;
            return true;
        } else if (key == KEY_LEFT) {
            tui_ConfigPrevTab();
            return true;
        } else if (key == KEY_RIGHT) {
            tui_ConfigNextTab();
            return true;
        } else if (key == '\n' || key == KEY_ENTER) {
            tui_ConfigEnterEdit();
            return true;
        }
    }
    return false;
}

// --- Updates & Rendering ---

/**
 * @brief Updates application state (stats, logic) before rendering.
 */
static void tui_Update(void) {
    // Mock Stats Update (Replace with real data later)
    g_tuiState.fps = 30.0f; 
    g_tuiState.cpuUsage = (float)(rand() % 1000) / 10.0f;
    g_tuiState.memUsage = 100 + (float)(rand() % 50);

    // Config Scroll Tracking (Auto-scroll to keep selection in view ONLY on change)
    if (g_tuiState.currentScreen == SCREEN_CONFIG) {
        if (g_tuiState.selectedConfigItemIndex != g_tuiState.lastSelectedConfigItemIndex) {
            g_tuiState.lastSelectedConfigItemIndex = g_tuiState.selectedConfigItemIndex;

            Clay_Dimensions dims = Clay_Ncurses_GetLayoutDimensions();

            // Item Height: 2 * TUI_CH (ConfigItemRow height) + 0 gap
            float itemHeight = 2.0f * TUI_CH;

            // View Height: Total - Headers/Footers
            // TopBar: ~3, Tabs: 3, Toolbar: 2, BottomBar: ~3, Padding: 1? => ~12
            float viewHeight = dims.height - (12.0f * TUI_CH); 
            if (viewHeight < itemHeight) viewHeight = itemHeight;

            float targetY = (float)g_tuiState.selectedConfigItemIndex * itemHeight;

            if (targetY < g_tuiState.configScrollY) {
                g_tuiState.configScrollY = targetY;
            } else if (targetY + itemHeight > g_tuiState.configScrollY + viewHeight) {
                g_tuiState.configScrollY = targetY + itemHeight - viewHeight;
            }
        }
    }
}

/**
 * @brief Main rendering routine.
 * 
 * Builds the Clay layout tree and dispatches render commands to Ncurses.
 */
static void tui_Render(void) {
    Clay_SetLayoutDimensions(Clay_Ncurses_GetLayoutDimensions());
    Clay_BeginLayout();

    CLAY(CLAY_ID("Root"),
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM},
        .backgroundColor = {0, 0, 0, 255}}) {

        tui_RenderTopBar();

        CLAY(CLAY_ID("MainArea"), 
            {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childGap = 0}}) {

            tui_RenderSidebar();
            RenderContent();
        }

        RenderErrorOverlay();
        tui_RenderBottomBar();
    }

    Clay_RenderCommandArray commands = Clay_EndLayout();
    Clay_Ncurses_Render(commands);
}

// --- Sidebar ---
static void tui_RenderSidebar(void) {
    const char *screenNames[] = { "Simulation", "Performance", "Logs", "Config" };

    CLAY(CLAY_ID("Sidebar"),

        {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(20 * TUI_CW), .height = CLAY_SIZING_GROW()},
                    .padding = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH},
                    .childGap = 1 * TUI_CH,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM},
         .backgroundColor = {0, 0, 0, 255},
         .border = {.width = {0, 1, 0, 0, 0}, .color = {50, 50, 50, 255}}}) {

        for (int i = 0; i < SCREEN_COUNT; i++) {
            bool isActive = (g_tuiState.currentScreen == (tuiScreen)i);
            CLAY(CLAY_ID_IDX("SidebarItem", (uint32_t)i),
                {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT()},
                            .padding = {1 * TUI_CW, 1 * TUI_CW, 0, 0}}}) {

                Clay_Ncurses_OnClick(OnSidebarClick, (void*)(intptr_t)i);
                bool isHovered = Clay_Hovered();

                CLAY_TEXT(CLAY_STRING_DYN((char*)screenNames[i]), 
                    CLAY_TEXT_CONFIG({
                        .textColor = (isActive || isHovered) ? COLOR_ACCENT : (Clay_Color){150, 150, 150, 255},
                        .fontId = isActive ? CLAY_NCURSES_FONT_BOLD : 0
                    }));
            }
        }

    }
}

static void OnSidebarClick(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
    (void)elementId;
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        int index = (int)(intptr_t)userData;
        if (index >= 0 && index < SCREEN_COUNT) {
            g_tuiState.currentScreen = (tuiScreen)index;
        }
    }
}




// --- Layout Definitions ---

/**
 * @brief Renders the main content area based on the selected screen.
 */
static void RenderContent(void) {
    CLAY(CLAY_ID("ContentBody"),
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .padding = {1 * TUI_CW, 1 * TUI_CW, 0, 0}}}) {
        if (g_tuiState.currentScreen == SCREEN_SIMULATION) {
            tui_RenderSimulationScreen();
        } else if (g_tuiState.currentScreen == SCREEN_PERFORMANCE) {
            tui_RenderPerformanceScreen();
        } else if (g_tuiState.currentScreen == SCREEN_LOGS) {
            tui_RenderLogsScreen();
        } else if (g_tuiState.currentScreen == SCREEN_CONFIG) {
            tui_RenderConfigScreen();
        }
    }
}

/**
 * @brief Renders an error overlay if `g_tuiState.showError` is true.
 */
static void RenderErrorOverlay(void) {
    if (!g_tuiState.showError) return;

    CLAY(CLAY_ID("ErrorOverlay"),
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(3 * TUI_CH)},
                    .padding = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}},
         .backgroundColor = COLOR_ERROR}) {
        CLAY_TEXT(CLAY_STRING_DYN(g_tuiState.errorMessage), CLAY_TEXT_CONFIG({.textColor = {0, 0, 0, 255}}));
    }
}
