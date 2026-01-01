#include "data_table.h"
#include <clay/clay.h>
#include <renderer/clay_ncurses_renderer.h>
#include "../tui_state.h" // For shared colors constants

// Helper logic for IDs
// Header: TableHeader_<ColID>
// Row:    TableRow_<RowIndex>
// Cell:   TableCell_<RowIndex>_<ColID>

// Global context for callbacks (acceptable for single-threaded TUI)
static struct {
    DataTableState *state;
    const DataTableDef *def;
    void *userAdapterData;
} g_TableContext;

static void HandleHeaderClick(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
    (void)elementId;
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        uint32_t colId = (uint32_t)(intptr_t)userData;

        if (g_TableContext.state->sortColumnId == colId) {
            g_TableContext.state->sortAscending = !g_TableContext.state->sortAscending;
        } else {
            g_TableContext.state->sortColumnId = colId;
            g_TableContext.state->sortAscending = true;
        }

        if (g_TableContext.def->adapter.OnSort) {
            g_TableContext.def->adapter.OnSort(g_TableContext.userAdapterData, colId, g_TableContext.state->sortAscending);
        }
    }
}

static void HandleRowClick(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
    (void)elementId;
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        int rowIndex = (int)(intptr_t)userData;
        g_TableContext.state->selectedRowIndex = rowIndex;

        if (g_TableContext.def->adapter.OnRowSelect) {
            g_TableContext.def->adapter.OnRowSelect(g_TableContext.userAdapterData, rowIndex);
        }
    }
}

bool tui_DataTableHandleInput(DataTableState *state, const DataTableDef *def, void *userData, int key) {
    if (key == ERR) return false;

    // Get row count via adapter if possible
    uint32_t rowCount = 0;
    if (def && def->adapter.GetCount) {
        rowCount = def->adapter.GetCount(userData);
    }

    // Selection (Enter)
    if (key == '\n' || key == KEY_ENTER) {
        if (state->selectedRowIndex >= 0 && state->selectedRowIndex < (int)rowCount) {
            if (def->adapter.OnRowSelect) {
                def->adapter.OnRowSelect(userData, state->selectedRowIndex);
            }
            return true;
        }
    }

    // Vertical Navigation
    if (key == KEY_DOWN) {
        if (state->selectedRowIndex < (int)rowCount - 1) {
            state->selectedRowIndex++;
            // Keep in view
            float selectionY = (float)state->selectedRowIndex * TUI_CH;
            // Assuming approx view height of 20 lines for auto-scroll
            if (selectionY + state->scrollY > 20 * TUI_CH) {
                state->scrollY -= TUI_CH;
            }
            return true;
        }
        return false; // Boundary reached, allow bubbling
    } 
    if (key == KEY_UP) {
        if (state->selectedRowIndex > 0) {
            state->selectedRowIndex--;
            if ((float)state->selectedRowIndex * TUI_CH < -state->scrollY) {
                state->scrollY += TUI_CH;
            }
            return true;
        }
        return false; // Boundary reached, allow bubbling
    }
    if (key == KEY_PPAGE) { // Page Up
        state->selectedRowIndex -= 20;
        if (state->selectedRowIndex < 0) state->selectedRowIndex = 0;
        state->scrollY = -(float)state->selectedRowIndex * TUI_CH;
        if (state->scrollY > 0) state->scrollY = 0;
        return true;
    }
    if (key == KEY_NPAGE) { // Page Down
        state->selectedRowIndex += 20;
        if (state->selectedRowIndex >= (int)rowCount) state->selectedRowIndex = (int)rowCount - 1;
        float selectionY = (float)state->selectedRowIndex * TUI_CH;
        state->scrollY = 20 * TUI_CH - selectionY;
        if (state->scrollY > 0) state->scrollY = 0;
        return true;
    }

    // Scroll Events (Shared with ncurses renderer)
    if (key == CLAY_NCURSES_KEY_SCROLL_UP) {
        state->scrollY += 2 * TUI_CH;
        if (state->scrollY > 0) state->scrollY = 0;
        return true;
    }
    if (key == CLAY_NCURSES_KEY_SCROLL_DOWN) {
        state->scrollY -= 2 * TUI_CH;
        return true;
    }
    if (key == CLAY_NCURSES_KEY_SCROLL_LEFT || key == KEY_SLEFT) {
        state->scrollX += 2 * TUI_CW;
        if (state->scrollX > 0) state->scrollX = 0;
        return true;
    }
    if (key == CLAY_NCURSES_KEY_SCROLL_RIGHT || key == KEY_SRIGHT) {
        state->scrollX -= 2 * TUI_CW;
        return true;
    }

    return false;
}

void tui_RenderDataTable(const DataTableDef *def, DataTableState *state, void *userData) {
    // Setup context for callbacks
    g_TableContext.state = state;
    g_TableContext.def = def;
    g_TableContext.userAdapterData = userData;



    uint32_t rowCount = def->adapter.GetCount(userData);

    // Calculate Total Content Width
    float totalWidth = 0;
    for (uint32_t i = 0; i < def->columnCount; i++) {
        totalWidth += def->columns[i].width * TUI_CW;
    }
    state->contentWidth = totalWidth;

    int newHoverIndex = -1;

    CLAY(CLAY_ID("DataTable"),
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .padding = {TUI_CW, TUI_CW, TUI_CH, TUI_CH}}, // Account for borders
         .backgroundColor = {0,0,0,255},
         .border = {.width = {1,1,1,1,0}, .color = {60,60,60,255}}}) {

        // --- Header Row ---
        // Header needs to scroll horizontally with the body, but stay fixed vertically.
        // To achieve this in Clay (TUI), we can apply a negative child gap or just use 'childOffset'.
        // We'll wrap the header items in a container that has the horizontal offset.

        CLAY(CLAY_ID("TableHeaderClip"), 
             {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(2 * TUI_CH)}},
              .backgroundColor = {30, 30, 30, 255},
              .border = {.width = {0,0,0,1,0}, .color = {80,80,80,255}},
              .clip = {.horizontal = true, .vertical = false, .childOffset = {.x = state->scrollX}}}) { // Apply X scroll

            CLAY(CLAY_ID("TableHeaderContent"),
                {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(totalWidth), .height = CLAY_SIZING_GROW()},
                            .layoutDirection = CLAY_LEFT_TO_RIGHT,
                            .childGap = 0}}) {

                for (uint32_t i = 0; i < def->columnCount; i++) {
                    const DataTableColumn *col = &def->columns[i];

                    CLAY(CLAY_ID_IDX("HeaderItem", col->id),
                        {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(col->width * TUI_CW), .height = CLAY_SIZING_GROW()},
                                    .padding = {1 * TUI_CW, 1 * TUI_CW, 0, 0}}}) {

                        if (col->sortable) {
                            Clay_Ncurses_OnClick(HandleHeaderClick, (void*)(intptr_t)col->id);
                        }

                        bool isSorted = (state->sortColumnId == col->id);

                        // Display Label
                        CLAY_TEXT(CLAY_STRING_DYN((char*)col->label), 
                            CLAY_TEXT_CONFIG({.textColor = isSorted ? COLOR_ACCENT : (Clay_Color){200,200,200,255},
                                              .fontId = CLAY_NCURSES_FONT_BOLD, .fontSize = 16}));

                        // Display Sort Arrow
                        if (isSorted) {
                            CLAY_TEXT(state->sortAscending ? CLAY_STRING(" ^") : CLAY_STRING(" v"),
                                CLAY_TEXT_CONFIG({.textColor = COLOR_ACCENT}));
                        }
                    }
                }
            }
        }

        // --- Data Rows (Scrollable) ---
        CLAY(CLAY_ID("TableBodyClip"),
             {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .clip = {.horizontal = true, .vertical = true, 
                       .childOffset = {.x = state->scrollX, .y = state->scrollY }}}) {

            CLAY(CLAY_ID("TableBodyContent"),
                 {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(totalWidth), .height = CLAY_SIZING_GROW()}, // Height grows to fit rows
                             .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {

                for (int r = 0; r < (int)rowCount; r++) {
                    bool isSelected = (r == state->selectedRowIndex);

                    CLAY(CLAY_ID_IDX("TableRow", (uint32_t)r),
                        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(1 * TUI_CH)}, // Width matches container (totalWidth)
                                    .layoutDirection = CLAY_LEFT_TO_RIGHT},
                         .backgroundColor = isSelected ? (Clay_Color){20, 60, 100, 255} : (r % 2 == 0 ? (Clay_Color){10,10,10,255} : (Clay_Color){0,0,0,255})}) {

                        Clay_Ncurses_OnClick(HandleRowClick, (void*)(intptr_t)r);

                        // Check Hover (Last one wins logic)
                        if (Clay_Hovered()) {
                            newHoverIndex = r;
                        }

                        // Use persisted hover state from previous frame/end of loop
                        bool isHovered = (state->hoveredRowIndex == r);

                        // Render Cells
                        for (uint32_t c = 0; c < def->columnCount; c++) {
                            const DataTableColumn *col = &def->columns[c];
                            char *cellData = tui_ScratchAlloc(256); // Allocate full buffer
                            if (def->adapter.GetCellData) { 
                                def->adapter.GetCellData(userData, r, col->id, cellData, 256);
                            } else {
                                cellData[0] = '\0';
                            }

                            CLAY(CLAY_ID_IDX2("Cell", (uint32_t)r, c),
                                {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(col->width * TUI_CW), .height = CLAY_SIZING_GROW()},
                                            .padding = {1 * TUI_CW, 0, 0, 0}}}) {

                                CLAY_TEXT(CLAY_STRING_DYN(cellData), 
                                CLAY_TEXT_CONFIG({.textColor = isSelected ? (Clay_Color){255,255,255,255} : 
                                                                (isHovered ? COLOR_ACCENT : (Clay_Color){180,180,180,255}),
                                                    .fontSize = 16}));
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Update state for next frame
    state->hoveredRowIndex = newHoverIndex;
}
