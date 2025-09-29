/** \file command_field.h
 *  \ingroup tui
 *  \brief Interactive command input field handling user keystrokes, history
 *         navigation and submission to the control bridge.
 *
 *  Features
 *  --------
 *  - Line editing (insert/delete, cursor move, word jump).
 *  - History ring with search (prefix or incremental planned).
 *  - Command parsing hand-off to adapter_director / ipc layer.
 */

