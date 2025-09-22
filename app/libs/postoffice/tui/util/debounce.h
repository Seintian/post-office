/**
 * @file debounce.h
 * @ingroup tui_util
 * @brief Debounce utility to coalesce rapid events into a single callback.
 */

#ifndef POSTOFFICE_TUI_UTIL_DEBOUNCE_H
#define POSTOFFICE_TUI_UTIL_DEBOUNCE_H

#include <stdbool.h>

typedef void (*debounce_cb)(void *ud);

typedef struct debounce debounce;

debounce *debounce_create(double interval_ms, debounce_cb cb, void *ud);
void debounce_destroy(debounce *d);

/** Schedule or reschedule the debounce timer. */
void debounce_bump(debounce *d);
/** Called by timerwheel; triggers callback if due. Returns true if fired. */
bool debounce_tick(debounce *d, double now_ms);

#endif /* POSTOFFICE_TUI_UTIL_DEBOUNCE_H */
