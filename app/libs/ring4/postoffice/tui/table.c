#include "table.h"

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <tui/tui.h>

static void tui_table_draw(tui_widget_t *widget) {
    tui_table_t *table = (tui_table_t *)widget;
    tui_rect_t b = widget->bounds;

    // Calculate column widths
    // Simple fixed logic for now or respect weights
    int total_fixed = 0;
    float total_weight = 0;
    for (int i = 0; i < table->column_count; i++) {
        if (table->columns[i].weight <= 0)
            total_fixed += table->columns[i].width;
        else
            total_weight += table->columns[i].weight;
    }

    int avail_w = b.size.width - (table->show_grid ? table->column_count + 1 : 0);
    int remaining = avail_w - total_fixed;

    // Draw Headers
    int y = b.position.y;
    int x = b.position.x;

    if (table->show_headers) {
        mvhline(y, x, ' ', b.size.width); // Header bg
        int curr_x = x;
        if (table->show_grid)
            mvaddch(y, curr_x++, ACS_VLINE);

        for (int i = 0; i < table->column_count; i++) {
            int w = table->columns[i].width;
            if (table->columns[i].weight > 0) {
                w = (int)((float)remaining * (table->columns[i].weight / total_weight));
            }

            attron(A_BOLD | A_UNDERLINE);
            mvprintw(y, curr_x, " %-*s ", w - 2, table->columns[i].title);
            attroff(A_BOLD | A_UNDERLINE);
            curr_x += w;
            if (table->show_grid)
                mvaddch(y, curr_x++, ACS_VLINE);
        }

        // Separator line
        if (table->show_grid) {
            y++;
            // Clamp separator width to widget bounds to prevent overflow
            int width = curr_x - x;
            if (width > b.size.width)
                width = b.size.width;
            mvhline(y, x, ACS_HLINE, width);
            y++;
        } else {
            y++;
        }
    }

    // Draw Rows
    int visible_h = b.size.height - (y - b.position.y);
    table->visible_rows = visible_h;

    // Scroll
    if (table->selected_row < table->top_visible_row)
        table->top_visible_row = table->selected_row;
    if (table->selected_row >= table->top_visible_row + visible_h)
        table->top_visible_row = table->selected_row - visible_h + 1;
    if (table->top_visible_row < 0)
        table->top_visible_row = 0;

    for (int r = 0; r < visible_h; r++) {
        int row_idx = table->top_visible_row + r;
        if (row_idx >= table->row_count)
            break;

        int curr_x = x;
        int attrs = 0;
        if (row_idx == table->selected_row) {
            attrs |= (int)A_REVERSE;
            if (widget->has_focus)
                attrs |= (int)A_BOLD;
        }

        if (table->show_grid)
            mvaddch(y + r, curr_x++, ACS_VLINE);

        for (int c = 0; c < table->column_count; c++) {
            int w = table->columns[c].width;
            if (table->columns[c].weight > 0) {
                w = (int)((float)remaining * (table->columns[c].weight / total_weight));
            }

            const char *val = table->rows[row_idx][c];
            attron(attrs);
            // truncate or pad
            mvprintw(y + r, curr_x, " %-*.*s ", w - 2, w - 2, val ? val : "");
            attroff(attrs);

            curr_x += w;
            if (table->show_grid) {
                // If selected, we might want to draw separator with reverse too? usually no.
                mvaddch(y + r, curr_x++, ACS_VLINE);
            }
        }
    }
}

static bool tui_table_handle_event(tui_widget_t *widget, const tui_event_t *event) {
    tui_table_t *table = (tui_table_t *)widget;

    if (event->type == TUI_EVENT_KEY) {
        int key = event->data.key;
        if (key == KEY_UP) {
            if (table->selected_row > 0) {
                table->selected_row--;
                if (table->on_select)
                    table->on_select(table, table->selected_row, widget->user_data);
                tui_widget_draw(widget);
                refresh();
            }
            return true;
        } else if (key == KEY_DOWN) {
            if (table->selected_row < table->row_count - 1) {
                table->selected_row++;
                if (table->on_select)
                    table->on_select(table, table->selected_row, widget->user_data);
                tui_widget_draw(widget);
                refresh();
            }
            return true;
        }
    }
    return false;
}

static void tui_table_free_impl(tui_widget_t *widget) {
    tui_table_t *table = (tui_table_t *)widget;
    for (int i = 0; i < table->column_count; i++)
        free(table->columns[i].title);
    free(table->columns);

    for (int i = 0; i < table->row_count; i++) {
        for (int c = 0; c < table->column_count; c++)
            free(table->rows[i][c]);
        free(table->rows[i]);
    }
    free(table->rows);
    free(table);
}

tui_table_t *tui_table_create(tui_rect_t bounds) {
    tui_table_t *table = malloc(sizeof(tui_table_t));
    tui_widget_init(&table->base, TUI_WIDGET_CUSTOM); // Should define TUI_WIDGET_TABLE
    table->base.bounds = bounds;
    table->base.draw = tui_table_draw;
    table->base.handle_event = tui_table_handle_event;
    table->base.free = tui_table_free_impl;
    table->base.focusable = true;

    table->columns = NULL;
    table->column_count = 0;
    table->rows = NULL;
    table->row_count = 0;
    table->row_capacity = 0;
    table->selected_row = 0;
    table->top_visible_row = 0;
    table->show_headers = true;
    table->show_grid = true;
    table->on_select = NULL;

    return table;
}

void tui_table_add_column(tui_table_t *table, const char *title, int width, float weight) {
    if (!table)
        return;
    table->columns =
        realloc(table->columns, sizeof(tui_table_column_t) * (size_t)(table->column_count + 1));
    table->columns[table->column_count].title = strdup(title);
    table->columns[table->column_count].width = width;
    table->columns[table->column_count].weight = weight;
    table->column_count++;
}

void tui_table_add_row(tui_table_t *table, const char **cell_data) {
    if (!table)
        return;

    if (table->row_count >= table->row_capacity) {
        table->row_capacity = (table->row_capacity == 0) ? 16 : table->row_capacity * 2;
        table->rows = realloc(table->rows, sizeof(char **) * (size_t)table->row_capacity);
    }

    char **row = malloc(sizeof(char *) * (size_t)table->column_count);
    for (int i = 0; i < table->column_count; i++) {
        row[i] = cell_data[i] ? strdup(cell_data[i]) : NULL;
    }

    table->rows[table->row_count++] = row;
}

void tui_table_set_select_callback(tui_table_t *table, void (*callback)(tui_table_t *, int, void *),
                                   void *user_data) {
    if (table) {
        table->on_select = callback;
        table->base.user_data = user_data;
    }
}
