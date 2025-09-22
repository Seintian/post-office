/**
 * @file command_line.h
 * @ingroup tui_input
 * @brief Command line editor interface.
 */

#ifndef POSTOFFICE_TUI_INPUT_COMMAND_LINE_H
#define POSTOFFICE_TUI_INPUT_COMMAND_LINE_H

#include <stddef.h>

typedef struct command_line command_line; /* opaque */

command_line *cmdline_create(size_t capacity);
void cmdline_destroy(command_line *cl);

int cmdline_insert(command_line *cl, const char *utf8, size_t len);
int cmdline_backspace(command_line *cl);
int cmdline_move_left(command_line *cl);
int cmdline_move_right(command_line *cl);
int cmdline_get_text(const command_line *cl, char *out, size_t out_size);
int cmdline_clear(command_line *cl);
int cmdline_set_text(command_line *cl, const char *utf8);
size_t cmdline_cursor(const command_line *cl);
int cmdline_set_cursor(command_line *cl, size_t pos);
/* Optional simple history */
int cmdline_history_add(command_line *cl, const char *utf8);
int cmdline_history_prev(command_line *cl);
int cmdline_history_next(command_line *cl);

#endif /* POSTOFFICE_TUI_INPUT_COMMAND_LINE_H */
