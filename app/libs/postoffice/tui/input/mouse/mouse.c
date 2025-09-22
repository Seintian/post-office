/**
 * @file mouse.c
 * @ingroup tui_input
 * @brief HOW: Mouse sequence decode (SGR minimal stub).
 */

#include "input/mouse/mouse.h"

#include <stdio.h>
#include <string.h>

bool mouse_enable_reporting(void) {
    return true;
}
bool mouse_disable_reporting(void) {
    return true;
}

bool mouse_translate_sequence(const char *seq, struct ui_event *out_event) {
    (void)seq;
    (void)out_event;
    return false;
}

bool mouse_decode_sgr(const char *seq, mouse_event_decoded *out) {
    /* Parse sequences like: \x1b[<b;x;yM or m */
    if (!seq || !out)
        return false;
    const char *p = strstr(seq, "[<");
    if (!p)
        return false;
    p += 2;
    int b = 0, x = 0, y = 0;
    char term = 'm';
    if (sscanf(p, "%d;%d;%d%c", &b, &x, &y, &term) < 4)
        return false;
    out->button = (mouse_button)(b & 0x3);
    out->mods = 0;
    out->x = x - 1;
    out->y = y - 1;
    out->action = (term == 'M') ? MOUSE_PRESS : MOUSE_RELEASE;
    out->wheel_delta = 0;
    return true;
}
