/**
 * @file keymap.h
 * @ingroup tui_input
 * @brief Key to command mapping interface.
 */

#ifndef POSTOFFICE_TUI_INPUT_KEYMAP_H
#define POSTOFFICE_TUI_INPUT_KEYMAP_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*keymap_log_fn)(void *ud, const char *msg);

struct ui_event; /* from core/ui_events.h (forward-declared) */
enum ui_command; /* from core/ui_commands.h (forward-declared) */

/** Initialize default key mappings (optionally with diagnostics). */
void keymap_init_defaults(void);
void keymap_set_logger(keymap_log_fn fn, void *ud);

/**
 * Translate a raw key event to a high-level command, if mapped.
 * @param ev input event (key)
 * @param consumed optional out flag if event was consumed by mapping
 * @return mapped command or UI_CMD_NONE
 */
enum ui_command keymap_translate(const struct ui_event *ev, bool *consumed);
/** Override mapping: map a key code (and modifiers) to a command. */
bool keymap_bind(uint32_t key, uint16_t mods, enum ui_command cmd);
/** Clear a mapping for a key/mods pair. */
bool keymap_unbind(uint32_t key, uint16_t mods);

#endif /* POSTOFFICE_TUI_INPUT_KEYMAP_H */
