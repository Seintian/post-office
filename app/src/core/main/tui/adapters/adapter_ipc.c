#include "../components/data_table.h"
#include "../tui_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t IPC_GetCount(void *userData) {
    (void)userData;
    return g_tuiState.mockIPCChannelCount;
}

static void IPC_GetCellData(void *userData, int row, uint32_t colId, char *outBuffer, size_t bufSize) {
    (void)userData;
    if (row < 0 || row >= (int)g_tuiState.mockIPCChannelCount) {
        snprintf(outBuffer, bufSize, "ERR");
        return;
    }
    
    MockIPCChannel *ch = &g_tuiState.mockIPCChannels[row];
    MockIPCNode *from = &g_tuiState.mockIPCNodes[ch->fromNodeIndex];
    MockIPCNode *to = &g_tuiState.mockIPCNodes[ch->toNodeIndex];
    
    switch (colId) {
        case 0: // Source
            strncpy(outBuffer, from->name, bufSize);
            break;
        case 1: // Destination
            strncpy(outBuffer, to->name, bufSize);
            break;
        case 2: // Msg/s
            snprintf(outBuffer, bufSize, "%d", ch->messagesPerSec);
            break;
        case 3: // Bandwidth
            snprintf(outBuffer, bufSize, "%d B/s", ch->bandwidthBytesPerSec);
            break;
        case 4: // Buffer
            snprintf(outBuffer, bufSize, "%u%%", ch->bufferUsagePercent);
            break;
        default:
            strncpy(outBuffer, "?", bufSize);
            break;
    }
}

static void IPC_OnSort(void *userData, uint32_t colId, bool ascending) {
    (void)userData;
    uint32_t count = g_tuiState.mockIPCChannelCount;
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = 0; j < count - i - 1; j++) {
            MockIPCChannel *a = &g_tuiState.mockIPCChannels[j];
            MockIPCChannel *b = &g_tuiState.mockIPCChannels[j + 1];
            
            bool swap = false;
            int cmp = 0;
            switch(colId) {
                case 0: cmp = strcmp(g_tuiState.mockIPCNodes[a->fromNodeIndex].name, g_tuiState.mockIPCNodes[b->fromNodeIndex].name); break;
                case 1: cmp = strcmp(g_tuiState.mockIPCNodes[a->toNodeIndex].name, g_tuiState.mockIPCNodes[b->toNodeIndex].name); break;
                case 2: cmp = (a->messagesPerSec - b->messagesPerSec); break;
                case 3: cmp = (a->bandwidthBytesPerSec - b->bandwidthBytesPerSec); break;
                case 4: cmp = (int)(a->bufferUsagePercent - b->bufferUsagePercent); break;
            }
            
            if (ascending) {
                if (cmp > 0) swap = true;
            } else {
                if (cmp < 0) swap = true;
            }
            
            if (swap) {
                MockIPCChannel temp = *a;
                *a = *b;
                *b = temp;
            }
        }
    }
}

static void IPC_OnRowSelect(void *userData, int row) {
    (void)userData;
    (void)row;
    // No specific row action yet
}

const DataTableAdapter g_IPCadapter = {
    .GetCount = IPC_GetCount,
    .GetCellData = IPC_GetCellData,
    .OnSort = IPC_OnSort,
    .OnRowSelect = IPC_OnRowSelect
};
