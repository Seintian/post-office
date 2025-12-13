/**
 * @file ansi.h
 * @brief ANSI escape sequences for terminal control.
 */

#ifndef TUI_ANSI_H
#define TUI_ANSI_H

/* Mouse Tracking */
#define TUI_ANSI_MOUSE_TRACKING_ENABLE  "\033[?1003h"
#define TUI_ANSI_MOUSE_TRACKING_DISABLE "\033[?1003l"

/* Cursor Visibility */
#define TUI_ANSI_CURSOR_SHOW "\033[?25h"
#define TUI_ANSI_CURSOR_HIDE "\033[?25l"

/* Screen Clearing */
#define TUI_ANSI_CLEAR_SCREEN "\033[2J"
#define TUI_ANSI_CURSOR_HOME  "\033[H"

#endif // TUI_ANSI_H
