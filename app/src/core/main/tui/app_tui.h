#ifndef APP_TUI_H
#define APP_TUI_H

#include <stdbool.h>

/**
 * @brief Runs the TUI in demo mode.
 *
 * Currently checks if the TUI system initializes correctly and runs the simulation loop.
 * This is effectively an alias for app_tui_run_simulation().
 */
void app_tui_run_demo(void);

/**
 * @brief Runs the TUI for the main simulation.
 *
 * Initializes the TUI, sets up the main layout (sidebar, topbar, content area),
 * registers global event handlers, and enters the main event loop.
 *
 * The layout consists of:
 * - A top bar for status.
 * - A sidebar for navigation between screens (Director, Ticket Issuer, etc.).
 * - A main content area that updates based on sidebar selection.
 */
void app_tui_run_simulation(void);

#endif
