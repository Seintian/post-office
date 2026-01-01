#ifndef DATA_TABLE_H
#define DATA_TABLE_H

#include <clay/clay.h>
#include <stdbool.h>
#include <stdint.h>

// Max columns supported in a table definition
#define DATA_TABLE_MAX_COLUMNS 16

/**
 * @brief Definition of a single column in the table.
 */
typedef struct {
    uint32_t id;          // Unique ID for the column (passed to adapter)
    const char *label;    // Header text
    float width;          // Fixed width (in TUI_CW units, e.g. 10 = 10 chars wide)
    bool sortable;        // If true, header is clickable for sorting
} DataTableColumn;

/**
 * @brief Interface for fetching data and handling interactions.
 * Application logic implements this to feed the table.
 */
typedef struct {
    /**
     * @brief Returns the total number of rows.
     */
    uint32_t (*GetCount)(void *userData);

    /**
     * @brief Fetches text content for a specific cell.
     * @param userData Custom data passed to render.
     * @param row Row index (0-based).
     * @param colId Column ID as defined in DataTableColumn.
     * @param outBuffer Buffer to write the string into.
     * @param bufSize Size of the buffer.
     */
    void (*GetCellData)(void *userData, int row, uint32_t colId, char *outBuffer, size_t bufSize);

    /**
     * @brief Callback when a sortable header is clicked.
     * Table state updates automatically, but app might need to resort its data.
     */
    void (*OnSort)(void *userData, uint32_t colId, bool ascending);

    /**
     * @brief Callback when a row is clicked.
     */
    void (*OnRowSelect)(void *userData, int row);
} DataTableAdapter;

/**
 * @brief Static definition of table structure.
 */
typedef struct {
    DataTableColumn columns[DATA_TABLE_MAX_COLUMNS];
    uint32_t columnCount;
    DataTableAdapter adapter;
} DataTableDef;

/**
 * @brief Mutable state of the table (scrolling, sorting, selection).
 * Should be persisted in the application state.
 */
typedef struct {
    float scrollY;            // Current vertical scroll offset
    float scrollX;            // Current horizontal scroll offset
    float contentWidth;       // Total width of content
    uint32_t sortColumnId;    // Currently sorted column ID
    bool sortAscending;       // Sort direction
    int selectedRowIndex;     // Currently selected row (-1 for none)
    int hoveredRowIndex;      // Currently hovered row (internal use mostly)
} DataTableState;

/**
 * @brief Handles keyboard and scroll input for a DataTable.
 * 
 * @param state The state to update.
 * @param rowCount Total number of rows in the table.
 * @param key The key code or scroll event code.
 * @return true if the input was consumed by the table.
 */
bool tui_DataTableHandleInput(DataTableState *state, const DataTableDef *def, void *userData, int key);

/**
 * @brief Renders the Generic Data Table.
 * 
 * @param def Table definition (columns, adapter).
 * @param state Pointer to mutable state.
 * @param userData Custom pointer passed to adapter functions.
 */
void tui_RenderDataTable(const DataTableDef *def, DataTableState *state, void *userData);

#endif // DATA_TABLE_H
