#include "screen_ipc.h"
#include "../tui_state.h"
#include "../components/data_table.h"
#include <clay/clay.h>
#include <renderer/clay_ncurses_renderer.h>
#include <stdio.h>
#include <stdlib.h>

extern const DataTableAdapter g_IPCadapter;

static const DataTableDef g_IPCTableDef = {
    .columns = {
        {0, "Source", 20.0f, true},
        {1, "Destination", 20.0f, true},
        {2, "Msg/s", 10.0f, true},
        {3, "Bandwidth", 15.0f, true},
        {4, "Buffer", 12.0f, true}
    },
    .columnCount = 5,
};

static DataTableDef g_ActiveIPCTableDef;
static bool g_IPCInitialized = false;

static void InitIPCTableDef(void) {
    g_ActiveIPCTableDef = g_IPCTableDef;
    g_ActiveIPCTableDef.adapter = g_IPCadapter;
    g_IPCInitialized = true;
}

void tui_InitIPCScreen(void) {
    g_tuiState.mockIPCNodeCount = 0;
    
    // Create Nodes
    MockIPCNode *nodes = g_tuiState.mockIPCNodes;
    
    snprintf(nodes[0].name, 32, "Director");
    snprintf(nodes[0].type, 32, "Director");
    nodes[0].active = true;
    
    snprintf(nodes[1].name, 32, "Ticket Issuer");
    snprintf(nodes[1].type, 32, "Issuer");
    nodes[1].active = true;
    
    snprintf(nodes[2].name, 32, "Users Mgr");
    snprintf(nodes[2].type, 32, "Manager");
    nodes[2].active = true;

    for (int i = 0; i < 4; i++) {
        snprintf(nodes[3+i].name, 32, "Worker-%d", i+1);
        snprintf(nodes[3+i].type, 32, "Worker");
        nodes[3+i].active = true;
    }
    g_tuiState.mockIPCNodeCount = 7;

    // Create Channels
    g_tuiState.mockIPCChannelCount = 0;
    MockIPCChannel *chans = g_tuiState.mockIPCChannels;

    // Director -> Issuer
    chans[0] = (MockIPCChannel){0, 1, 0, 0, 0};
    // Director -> UsersMgr
    chans[1] = (MockIPCChannel){0, 2, 0, 0, 0};

    // Issuer -> Workers (Fan out)
    for (int i = 0; i < 4; i++) {
        chans[2+i] = (MockIPCChannel){1, 3+i, 0, 0, 0};
    }
    g_tuiState.mockIPCChannelCount = 6;
    
    g_tuiState.ipcTableState = (DataTableState){0, 0, 0, 0, true, -1, -1};
}

void tui_UpdateIPCScreen(void) {
    // Simulate Traffic
    for (uint32_t i = 0; i < g_tuiState.mockIPCChannelCount; i++) {
        MockIPCChannel *ch = &g_tuiState.mockIPCChannels[i];
        
        int delta = (rand() % 11) - 5; 
        ch->messagesPerSec += delta;
        if (ch->messagesPerSec < 0) ch->messagesPerSec = 0;
        if (ch->messagesPerSec > 1000) ch->messagesPerSec = 1000;

        ch->bandwidthBytesPerSec = ch->messagesPerSec * 64;
        ch->bufferUsagePercent = (uint32_t)((float)ch->messagesPerSec / 10.0f);
    }
}

void tui_RenderIPCScreen(void) {
    if (!g_IPCInitialized) InitIPCTableDef();

    CLAY(CLAY_ID("IPCWrapper"), 
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .childGap = 1 * TUI_CH,
                    .padding = {2 * TUI_CW, 2 * TUI_CW, 1 * TUI_CH, 1 * TUI_CH}}}) {

        CLAY_TEXT(CLAY_STRING("Network / IPC Status"), CLAY_TEXT_CONFIG({.textColor = COLOR_ACCENT, .fontId = CLAY_NCURSES_FONT_BOLD}));
        
        // Render the generic DataTable
        tui_RenderDataTable(&g_ActiveIPCTableDef, &g_tuiState.ipcTableState, NULL);
    }
}

bool tui_IPCHandleInput(int key) {
    if (!g_IPCInitialized) InitIPCTableDef();
    return tui_DataTableHandleInput(&g_tuiState.ipcTableState, &g_ActiveIPCTableDef, NULL, key);
}
