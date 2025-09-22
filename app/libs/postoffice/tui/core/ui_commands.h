/**
 * @file ui_commands.h
 * @ingroup tui_core
 * @brief WHAT: High-level user intent commands and posting API.
 */

#ifndef POSTOFFICE_TUI_CORE_UI_COMMANDS_H
#define POSTOFFICE_TUI_CORE_UI_COMMANDS_H

enum ui_command {
    UI_CMD_NONE = 0,
    UI_CMD_FOCUS_NEXT,
    UI_CMD_FOCUS_PREV,
    UI_CMD_ACTIVATE,
    UI_CMD_CANCEL,
    UI_CMD_NAV_UP,
    UI_CMD_NAV_DOWN,
    UI_CMD_NAV_LEFT,
    UI_CMD_NAV_RIGHT,
    UI_CMD_PAGE_UP,
    UI_CMD_PAGE_DOWN,
    UI_CMD_HOME,
    UI_CMD_END,
};

struct ui_context;

/** Post a command for the active screen to handle. */
typedef bool (*ui_command_handler)(void *ud, enum ui_command cmd, void *payload);

bool ui_commands_post(struct ui_context *ctx, enum ui_command cmd, void *payload);
void ui_commands_set_handler(struct ui_context *ctx, ui_command_handler h, void *ud);

#endif /* POSTOFFICE_TUI_CORE_UI_COMMANDS_H */
