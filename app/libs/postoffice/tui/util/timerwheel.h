/**
 * @file timerwheel.h
 * @ingroup tui_util
 * @brief Hierarchical timer wheel for timeouts/intervals.
 */

#ifndef POSTOFFICE_TUI_UTIL_TIMERWHEEL_H
#define POSTOFFICE_TUI_UTIL_TIMERWHEEL_H

#include <stdbool.h>

typedef void (*timer_cb)(void *ud);

typedef struct timerwheel timerwheel;

typedef struct timer_id {
    unsigned long long id;
} timer_id;

timerwheel *timerwheel_create(void);
void timerwheel_destroy(timerwheel *tw);

/** Schedule a one-shot timer at now+delay_ms */
timer_id timerwheel_add(timerwheel *tw, double now_ms, double delay_ms, timer_cb cb, void *ud);
/** Schedule a repeating timer */
timer_id timerwheel_add_interval(timerwheel *tw, double now_ms, double interval_ms, timer_cb cb,
                                 void *ud);
/** Cancel a timer (no-op if not found) */
void timerwheel_cancel(timerwheel *tw, timer_id id);
/** Advance time and fire due timers. */
void timerwheel_tick(timerwheel *tw, double now_ms);

#endif /* POSTOFFICE_TUI_UTIL_TIMERWHEEL_H */
