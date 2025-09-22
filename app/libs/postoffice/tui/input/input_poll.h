/**
 * @file input_poll.h
 * @ingroup tui_input
 * @brief Terminal input polling and decode layer.
 *
 * WHAT: Polls the backend (ncurses) for key/mouse/resize and emits `ui_event`s.
 * HOW (see `input_poll.c`):
 *  - Uses blocking or non-blocking `getch()` depending on loop strategy.
 *  - Decodes escape sequences; mouse planned via SGR/X10 parsing.
 */

#ifndef POSTOFFICE_TUI_INPUT_INPUT_POLL_H
#define POSTOFFICE_TUI_INPUT_INPUT_POLL_H

#include <stdbool.h>

struct ui_context;

/** Poll OS/terminal input and enqueue events into the engine. */
bool input_poll_step(struct ui_context *ctx, double max_block_ms);

void input_poll_set_nonblocking(bool nonblocking);

#endif /* POSTOFFICE_TUI_INPUT_INPUT_POLL_H */
