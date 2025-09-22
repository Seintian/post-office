/**
 * @file delta_clock.h
 * @ingroup tui_util
 * @brief Monotonic clock wrapper producing frame deltas.
 */

#ifndef POSTOFFICE_TUI_UTIL_DELTA_CLOCK_H
#define POSTOFFICE_TUI_UTIL_DELTA_CLOCK_H

typedef struct delta_clock delta_clock;

delta_clock *delta_clock_create(void);
void delta_clock_destroy(delta_clock *dc);
/**
 * @return elapsed milliseconds since last call; initializes on first call.
 */
double delta_clock_tick(delta_clock *dc);

#endif /* POSTOFFICE_TUI_UTIL_DELTA_CLOCK_H */
