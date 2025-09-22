/**
 * @file mouse.h
 * @ingroup tui_input
 * @brief Mouse reporting parse API (minimal stubs).
 */

#ifndef POSTOFFICE_TUI_INPUT_MOUSE_H
#define POSTOFFICE_TUI_INPUT_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

struct ui_event; /* fwd */

typedef enum mouse_button {
    MOUSE_LEFT = 1,
    MOUSE_MIDDLE = 2,
    MOUSE_RIGHT = 3,
    MOUSE_WHEEL_UP = 4,
    MOUSE_WHEEL_DOWN = 5
} mouse_button;
typedef enum mouse_mods {
    MOUSE_MOD_SHIFT = 1u << 0,
    MOUSE_MOD_ALT = 1u << 1,
    MOUSE_MOD_CTRL = 1u << 2
} mouse_mods;
typedef enum mouse_action {
    MOUSE_PRESS,
    MOUSE_RELEASE,
    MOUSE_MOVE,
    MOUSE_DRAG,
    MOUSE_WHEEL
} mouse_action;

typedef struct mouse_event_decoded {
    mouse_action action;
    mouse_button button;
    uint16_t mods;
    int x, y;
    int wheel_delta; /* +/- for wheel */
} mouse_event_decoded;

bool mouse_enable_reporting(void);
bool mouse_disable_reporting(void);
bool mouse_translate_sequence(const char *seq, struct ui_event *out_event);
bool mouse_decode_sgr(const char *seq, mouse_event_decoded *out);

#endif /* POSTOFFICE_TUI_INPUT_MOUSE_H */
