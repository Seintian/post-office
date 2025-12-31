#include "app_tui.h"

#ifndef CLAY_IMPLEMENTATION
#include <clay/clay.h>
#endif

#include <renderer/clay_ncurses_renderer.h>
#include <utils/signals.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

// --- Constants & Enums ---

#define INPUT_BUFFER_SIZE 256
#define CTRL_KEY(k) ((k) & 0x1f)

typedef enum {
    SCREEN_SIMULATION,
    SCREEN_PERFORMANCE,
    SCREEN_COUNT
} tuiScreen;

typedef enum {
    TAB_SIM_DIRECTOR,
    TAB_SIM_USERS,
    TAB_SIM_COUNT
} SimTab;

typedef enum {
    TAB_PERF_SYSTEM,
    TAB_PERF_LIBRARIES,
    TAB_PERF_STATS,
    TAB_PERF_COUNT
} PerfTab;

typedef struct {
    // Navigation State
    tuiScreen currentScreen;
    int activeSimTab;
    int activePerfTab;

    // Input State
    char inputBuffer[INPUT_BUFFER_SIZE];
    int inputCursor;

    // System Stats (Mocked for now)
    float fps;
    float cpuUsage;
    float memUsage; // In MB

    // Control
    bool running;
    bool showError;
    char errorMessage[256];
} tuiState;

static tuiState g_tuiState = {
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

// --- Helper Macros ---

#define COLOR_ACCENT (Clay_Color) { 100, 200, 255, 255 }
#define COLOR_ERROR (Clay_Color) { 255, 100, 100, 255 }
#define COLOR_TEXT_DIM (Clay_Color) { 120, 120, 120, 255 }

#define TUI_CW 8   // Cell Width
#define TUI_CH 16  // Cell Height

#define CLAY_STRING_DYN(s) (Clay_String) { .length = (int32_t)strlen(s), .chars = (s) }

// --- Internal Prototypes ---

static void tui_Initialize(void);
static void tui_Terminate(void);
static int tui_ProcessInput(void);
static void tui_Update(void);
static void tui_Render(void);

static void HandleKeyInput(int key);
static int tui_OnSuspend(void);
static void tui_OnQuit(void);
static int tui_OnInterrupt(void);
static int tui_OnCrash(void);

// --- Layouts ---
static void RenderHeader(void);
static void RenderFooter(void);
static void RenderContent(void);
static void RenderErrorOverlay(void);

// --- Core Lifecycle ---

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

static void tui_Initialize(void) {
    uint64_t totalMemorySize = 16 * 1024 * 1024; // 16 MB
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
    Clay_Initialize(arena, (Clay_Dimensions){0, 0}, (Clay_ErrorHandler){0});
    Clay_SetMeasureTextFunction(Clay_Ncurses_MeasureText, NULL);

    Clay_Ncurses_Initialize();
    Clay_Ncurses_SetRawMode(true); // capture all keys
}

static void tui_Terminate(void) {
    Clay_Ncurses_Terminate();
}

// --- Input & Signal Handling ---

static int tui_ProcessInput(void) {
    int key = Clay_Ncurses_ProcessInputStandard();

    // Check for Signal Keys first
    if (key == CTRL_KEY('q')) {
        tui_OnQuit();
    } else if (key == CTRL_KEY('c')) {
        return tui_OnInterrupt();
    } else if (key == CTRL_KEY('z')) {
        return tui_OnSuspend();
    } else if (key == CTRL_KEY('\\')) {
        return tui_OnCrash();
    } else {
        HandleKeyInput(key);
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

static void HandleKeyInput(int key) {
    if (key == ERR) return;

    // Navigation
    if (key == KEY_F(1)) {
        g_tuiState.currentScreen = SCREEN_SIMULATION;
        return;
    }
    if (key == KEY_F(2)) {
        g_tuiState.currentScreen = SCREEN_PERFORMANCE;
        return;
    }

    // Tab Switching
    if (key == '\t') {
        if (g_tuiState.currentScreen == SCREEN_SIMULATION) {
            g_tuiState.activeSimTab = (g_tuiState.activeSimTab + 1) % TAB_SIM_COUNT;
        } else {
            g_tuiState.activePerfTab = (g_tuiState.activePerfTab + 1) % TAB_PERF_COUNT;
        }
        return;
    }

    // Text Input Command Dispatch
    if (key == '\n' || key == KEY_ENTER) {
        if (g_tuiState.inputCursor > 0) {
            // TODO: dispatch command (ctrl_bridge)
            g_tuiState.inputCursor = 0;
            g_tuiState.inputBuffer[0] = '\0';
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

// --- Updates & Rendering ---

static void tui_Update(void) {
    // Mock Stats Update (Replace with real data later)
    g_tuiState.fps = 30.0f; 
    g_tuiState.cpuUsage = (float)(rand() % 1000) / 10.0f;
    g_tuiState.memUsage = 100 + (float)(rand() % 50);
}

static void tui_Render(void) {
    Clay_SetLayoutDimensions(Clay_Ncurses_GetLayoutDimensions());
    Clay_BeginLayout();

    CLAY(CLAY_ID("Root"),
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM},
        .backgroundColor = {0, 0, 0, 255}}) {

        RenderHeader();
        RenderContent();
        RenderErrorOverlay();
        RenderFooter();
    }

    Clay_RenderCommandArray commands = Clay_EndLayout();
    Clay_Ncurses_Render(commands);
}

// --- Layout Definitions ---

static void RenderHeader(void) {
    CLAY(CLAY_ID("Header"),
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(3 * TUI_CH)},
                     .padding = {2 * TUI_CW, 2 * TUI_CW, 0, 0},
                     .childGap = 4 * TUI_CW,
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .border = {.width = {0, 0, 0, 1 * TUI_CH}, .color = {255, 255, 255, 255}}}) {
        
        CLAY_TEXT(CLAY_STRING("POST OFFICE"),
                  CLAY_TEXT_CONFIG({.fontId = CLAY_NCURSES_FONT_BOLD}));

        const char *screenName = (g_tuiState.currentScreen == SCREEN_SIMULATION) ? "SIMULATION" : "PERFORMANCE";
        CLAY(CLAY_ID("ScreenTitle"), {.layout = {.padding = {1 * TUI_CW, 1 * TUI_CW, 0, 0}}}) {
             CLAY_TEXT(CLAY_STRING_DYN((char*)screenName), CLAY_TEXT_CONFIG({.textColor = {150, 255, 150, 255}}));
        }

        CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW()}}}); 

        static char statsBuffer[64];
        snprintf(statsBuffer, sizeof(statsBuffer), "FPS: %.0f | CPU: %.1f%% | MEM: %.0fMB", 
                 (double)g_tuiState.fps, (double)g_tuiState.cpuUsage, (double)g_tuiState.memUsage);
        CLAY_TEXT(CLAY_STRING_DYN(statsBuffer), CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT_DIM}));
    }
}

static void RenderFooter(void) {
    CLAY(CLAY_ID("Footer"),
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(3 * TUI_CH)},
                     .padding = {2 * TUI_CW, 2 * TUI_CW, 0, 0},
                     .childGap = 0,
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .border = {.width = {0, 0, 1 * TUI_CH, 0}, .color = {255, 255, 255, 255}}}) {

        CLAY_TEXT(CLAY_STRING("> "), CLAY_TEXT_CONFIG({.fontId = CLAY_NCURSES_FONT_BOLD, .textColor = {0, 255, 0, 255}}));

        CLAY(CLAY_ID("InputContainer"), 
            {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(50 * TUI_CW), .height = CLAY_SIZING_FIXED(1 * TUI_CH)},
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childAlignment = {.y = CLAY_ALIGN_Y_TOP}}}) {

            int viewWidth = 50;
            int start = (g_tuiState.inputCursor > viewWidth) ? g_tuiState.inputCursor - viewWidth : 0;
            Clay_String inputStr = CLAY_STRING_DYN(&g_tuiState.inputBuffer[start]);

            CLAY_TEXT(inputStr, CLAY_TEXT_CONFIG({.textColor = {0, 255, 0, 255}, .fontId = CLAY_NCURSES_FONT_BOLD}));

            CLAY(CLAY_ID("Cursor"), 
                {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(1 * TUI_CW), .height = CLAY_SIZING_FIXED(1 * TUI_CH)}},
                 .backgroundColor = {0, 255, 0, 255}});
        }

        CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW()}}}); 

        CLAY_TEXT(CLAY_STRING("[F1] Sim  [F2] Perf  [TAB] Switch Tab  [Ctrl+q] Quit"), 
                  CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT_DIM}));
    }
}

static void RenderTabs(const char *titles[], int count, int activeIndex) {
    CLAY_AUTO_ID(
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(3 * TUI_CH)},
                     .childGap = 2 * TUI_CW,
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .padding = {1 * TUI_CW, 0, 1 * TUI_CW, 0}}}) {
        for (int i = 0; i < count; i++) {
            bool isActive = (i == activeIndex);
            CLAY_AUTO_ID( 
                {.layout = {.padding = {2 * TUI_CW, 2 * TUI_CW, 0, 0}},
                 .border = {.width = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}, 
                            .color = isActive ? COLOR_ACCENT : COLOR_TEXT_DIM}}) {
                 
                 CLAY_TEXT(CLAY_STRING_DYN((char*)titles[i]), 
                        CLAY_TEXT_CONFIG({.textColor = isActive ? COLOR_ACCENT : COLOR_TEXT_DIM,
                                          .fontId = isActive ? CLAY_NCURSES_FONT_BOLD : 0}));
            }
        }
    }
}

static void RenderSimulationScreen(void) {
    const char *tabs[] = {"Director", "Users"};
    RenderTabs(tabs, 2, g_tuiState.activeSimTab);

    CLAY(CLAY_ID("SimContent"), 
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .padding = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}}}) {

        if (g_tuiState.activeSimTab == TAB_SIM_DIRECTOR) {
            CLAY_TEXT(CLAY_STRING("Director View - Placeholder"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}}));
        } else {
            CLAY_TEXT(CLAY_STRING("Users View - Placeholder"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}}));
        }
    }
}

static void RenderPerformanceScreen(void) {
    const char *tabs[] = {"System", "Libraries", "Stats"};
    RenderTabs(tabs, 3, g_tuiState.activePerfTab);

    CLAY(CLAY_ID("PerfContent"), 
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .padding = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}}}) {
        CLAY_TEXT(CLAY_STRING("Performance Metrics - Placeholder"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}}));
    }
}

static void RenderContent(void) {
    CLAY(CLAY_ID("ContentBody"),
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .padding = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}},
         .border = {.width = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}, 
                    .color = {255, 255, 255, 255}}}) {
        if (g_tuiState.currentScreen == SCREEN_SIMULATION) {
            RenderSimulationScreen();
        } else {
            RenderPerformanceScreen();
        }
    }
}

static void RenderErrorOverlay(void) {
    if (!g_tuiState.showError) return;

    CLAY(CLAY_ID("ErrorOverlay"),
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(3 * TUI_CH)},
                    .padding = {1 * TUI_CW, 1 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}},
         .backgroundColor = COLOR_ERROR}) {
        CLAY_TEXT(CLAY_STRING_DYN(g_tuiState.errorMessage), CLAY_TEXT_CONFIG({.textColor = {0, 0, 0, 255}}));
    }
}
