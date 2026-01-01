#include "../components/data_table.h"
#include "../tui_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- DataTableAdapter Implementation ---

// --- DataTableAdapter Implementation ---

static uint32_t Entities_GetCount(void *userData) {
    (void)userData;
    // Always use the filtered count. If no filter, it equals total.
    return g_tuiState.filteredEntityCount;
}

static void Entities_GetCellData(void *userData, int row, uint32_t colId, char *outBuffer, size_t bufSize) {
    (void)userData;
    if (row < 0 || row >= (int)g_tuiState.filteredEntityCount) {
        snprintf(outBuffer, bufSize, "ERR");
        return;
    }
    
    // Map view row to real entity index
    int realIndex = g_tuiState.filteredEntityIndices[row];
    MockEntity *e = &g_tuiState.mockEntities[realIndex];
    
    switch (colId) {
        case 0: // ID
            snprintf(outBuffer, bufSize, "%u", e->id);
            break;
        case 1: // Type
            switch (e->type) {
                case ENTITY_TYPE_DIRECTOR: strncpy(outBuffer, "Director", bufSize); break;
                case ENTITY_TYPE_MANAGER: strncpy(outBuffer, "Manager", bufSize); break;
                case ENTITY_TYPE_WORKER: strncpy(outBuffer, "Worker", bufSize); break;
                case ENTITY_TYPE_USER: strncpy(outBuffer, "User", bufSize); break;
            }
            break;
        case 2: // Name
            strncpy(outBuffer, e->name, bufSize);
            break;
        case 3: // State
            strncpy(outBuffer, e->state, bufSize);
            break;
        case 4: // Location
            strncpy(outBuffer, e->location, bufSize);
            break;
        case 5: // Queue
            snprintf(outBuffer, bufSize, "%d", e->queueDepth);
            break;
        case 6: // CPU
            snprintf(outBuffer, bufSize, "%.1f%%", (double)e->cpuUsage);
            break;
        default:
            strncpy(outBuffer, "?", bufSize);
            break;
    }
}

static void Entities_OnSort(void *userData, uint32_t colId, bool ascending) {
    (void)userData;
    // Sorting should sort the FILTERED indices based on the values they point to.
    // For simplicity in this mock, we'll Bubble Sort the filteredEntityIndices array.
    
    uint32_t count = g_tuiState.filteredEntityCount;
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = 0; j < count - i - 1; j++) {
            int idxA = g_tuiState.filteredEntityIndices[j];
            int idxB = g_tuiState.filteredEntityIndices[j + 1];
            MockEntity *a = &g_tuiState.mockEntities[idxA];
            MockEntity *b = &g_tuiState.mockEntities[idxB];
            
            bool swap = false;
            int cmp = 0;
            switch(colId) {
                case 0: cmp = (a->id > b->id) ? 1 : (a->id < b->id) ? -1 : 0; break;
                case 1: cmp = (int)a->type - (int)b->type; break;
                case 2: cmp = strcmp(a->name, b->name); break;
                case 3: cmp = strcmp(a->state, b->state); break;
                case 4: cmp = strcmp(a->location, b->location); break;
                case 5: cmp = (a->queueDepth - b->queueDepth); break;
                case 6: cmp = (a->cpuUsage > b->cpuUsage) ? 1 : (a->cpuUsage < b->cpuUsage) ? -1 : 0; break;
            }
            
            if (ascending) {
                if (cmp > 0) swap = true;
            } else {
                if (cmp < 0) swap = true;
            }
            
            if (swap) {
                g_tuiState.filteredEntityIndices[j] = idxB;
                g_tuiState.filteredEntityIndices[j + 1] = idxA;
            }
        }
    }
}

static void Entities_OnRowSelect(void *userData, int row) {
    (void)userData;
    if (row >= 0 && row < (int)g_tuiState.filteredEntityCount) {
        // Store the REAL index as selected, so modal shows correct entity
        g_tuiState.selectedEntityIndex = g_tuiState.filteredEntityIndices[row];
    }
}

const DataTableAdapter g_EntitiesAdapter = {
    .GetCount = Entities_GetCount,
    .GetCellData = Entities_GetCellData,
    .OnSort = Entities_OnSort,
    .OnRowSelect = Entities_OnRowSelect
};

// --- Logic ---

void tui_UpdateEntitiesFilter(void) {
    g_tuiState.filteredEntityCount = 0;
    const char *filter = g_tuiState.entitiesFilter;
    bool hasFilter = (strlen(filter) > 0);
    
    // Tab 0: System (Director, Managers)
    // Tab 1: Simulation (Workers, Users)
    uint32_t activeTab = g_tuiState.activeEntitiesTab;

    for (uint32_t i = 0; i < g_tuiState.mockEntityCount; i++) {
        MockEntity *e = &g_tuiState.mockEntities[i];
        bool match = true;
        
        // 1. Tab Filtering
        if (activeTab == 0) { // System
            if (e->type != ENTITY_TYPE_DIRECTOR && e->type != ENTITY_TYPE_MANAGER) match = false;
        } else { // Simulation
            if (e->type != ENTITY_TYPE_WORKER && e->type != ENTITY_TYPE_USER) match = false;
        }

        // 2. String Filtering
        if (match && hasFilter) {
            if (strstr(e->name, filter) == NULL) {
                 match = false;
            }
        }
        
        if (match) {
            g_tuiState.filteredEntityIndices[g_tuiState.filteredEntityCount++] = (int)i;
        }
    }
    // Check bounds
    if (g_tuiState.entitiesTableState.selectedRowIndex >= (int)g_tuiState.filteredEntityCount) {
        if (g_tuiState.filteredEntityCount > 0)
            g_tuiState.entitiesTableState.selectedRowIndex = 0;
        else
            g_tuiState.entitiesTableState.selectedRowIndex = -1;
    }
}

void tui_InitEntities(void) {
    g_tuiState.mockEntityCount = 0;
    
    // Director
    MockEntity *e = &g_tuiState.mockEntities[g_tuiState.mockEntityCount++];
    e->id = 1;
    e->type = ENTITY_TYPE_DIRECTOR;
    strcpy(e->name, "Director");
    strcpy(e->state, "Running");
    strcpy(e->location, "HQ");
    e->cpuUsage = 0.5f;
    e->queueDepth = 0;
    
    // Managers
    e = &g_tuiState.mockEntities[g_tuiState.mockEntityCount++];
    e->id = 2;
    e->type = ENTITY_TYPE_MANAGER;
    strcpy(e->name, "Ticket Issuer");
    strcpy(e->state, "Active");
    strcpy(e->location, "Entrance");
    e->cpuUsage = 1.2f;
    e->queueDepth = 12;

    e = &g_tuiState.mockEntities[g_tuiState.mockEntityCount++];
    e->id = 3;
    e->type = ENTITY_TYPE_MANAGER;
    strcpy(e->name, "Users Manager");
    strcpy(e->state, "Active");
    strcpy(e->location, "Backoffice");
    e->cpuUsage = 0.8f;
    e->queueDepth = 5;

    // Workers
    for (int i = 0; i < 8; i++) {
        e = &g_tuiState.mockEntities[g_tuiState.mockEntityCount++];
        e->id = 100 + (uint32_t)i;
        e->type = ENTITY_TYPE_WORKER;
        snprintf(e->name, 32, "Worker-%d", i+1);
        if (i < 5) {
            strcpy(e->state, "Working");
            snprintf(e->location, 32, "Counter %d", i+1);
        } else {
            strcpy(e->state, "Idle");
            strcpy(e->location, "Pool");
        }
        e->cpuUsage = ((float)(rand() % 50)) / 10.0f;
        e->queueDepth = (rand() % 5);
    }
    
    // Users
    for (int i = 0; i < 50; i++) {
        e = &g_tuiState.mockEntities[g_tuiState.mockEntityCount++];
        e->id = 1000 + (uint32_t)i;
        e->type = ENTITY_TYPE_USER;
        snprintf(e->name, 32, "User-%03d", i+1);
        if (i < 10) {
            strcpy(e->state, "Being Served");
            strcpy(e->location, "Counter");
        } else {
            strcpy(e->state, "Waiting");
            strcpy(e->location, "Lobby");
        }
        e->cpuUsage = 0.0f;
        e->queueDepth = 0;
        e->memoryUsageMB = 1;
    }
    
    // Init Indices
    tui_UpdateEntitiesFilter();
    
    // Defaults
    g_tuiState.selectedEntityIndex = -1;
    g_tuiState.entitiesTableState = (DataTableState){0};
}

void tui_UpdateEntities(void) {
    // Randomly fluctuate CPU/State of random entities
    if (g_tuiState.mockEntityCount > 0) {
        int idx = rand() % (int)g_tuiState.mockEntityCount;
        MockEntity *e = &g_tuiState.mockEntities[idx];
        
        if (e->type == ENTITY_TYPE_WORKER) {
            e->cpuUsage += ((float)(rand() % 10) - 5.0f) / 10.0f;
            if (e->cpuUsage < 0) e->cpuUsage = 0.1f;
        } else if (e->type == ENTITY_TYPE_USER) {
            // occasionally switch state
            if (rand() % 100 < 2) {
                if (strcmp(e->state, "Waiting") == 0) strcpy(e->state, "Looking around");
                else strcpy(e->state, "Waiting");
            }
        }
    }
}
