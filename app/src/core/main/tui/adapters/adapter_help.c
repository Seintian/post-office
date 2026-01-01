#include "../components/data_table.h"
#include "../tui_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t Help_GetCount(void *userData) {
    (void)userData;
    return g_tuiState.helpBindingCount;
}

static void Help_GetCellData(void *userData, int row, uint32_t colId, char *outBuffer, size_t bufSize) {
    (void)userData;
    if (row < 0 || row >= (int)g_tuiState.helpBindingCount) {
        snprintf(outBuffer, bufSize, "ERR");
        return;
    }
    
    Keybinding *kb = &g_tuiState.helpBindings[row];
    
    switch (colId) {
        case 0: // Key
            strncpy(outBuffer, kb->key, bufSize);
            break;
        case 1: // Description
            strncpy(outBuffer, kb->description, bufSize);
            break;
        case 2: // Context
            strncpy(outBuffer, kb->context, bufSize);
            break;
        default:
            strncpy(outBuffer, "?", bufSize);
            break;
    }
}

static void Help_OnSort(void *userData, uint32_t colId, bool ascending) {
    (void)userData;
    uint32_t count = g_tuiState.helpBindingCount;
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = 0; j < count - i - 1; j++) {
            Keybinding *a = &g_tuiState.helpBindings[j];
            Keybinding *b = &g_tuiState.helpBindings[j + 1];
            
            bool swap = false;
            int cmp = 0;
            switch(colId) {
                case 0: cmp = strcmp(a->key, b->key); break;
                case 1: cmp = strcmp(a->description, b->description); break;
                case 2: cmp = strcmp(a->context, b->context); break;
            }
            
            if (ascending) {
                if (cmp > 0) swap = true;
            } else {
                if (cmp < 0) swap = true;
            }
            
            if (swap) {
                Keybinding temp = *a;
                *a = *b;
                *b = temp;
            }
        }
    }
}

static void Help_OnRowSelect(void *userData, int row) {
    (void)userData;
    (void)row;
}

const DataTableAdapter g_HelpAdapter = {
    .GetCount = Help_GetCount,
    .GetCellData = Help_GetCellData,
    .OnSort = Help_OnSort,
    .OnRowSelect = Help_OnRowSelect
};
