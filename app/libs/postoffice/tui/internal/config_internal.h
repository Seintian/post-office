/**
 * @file config_internal.h
 * @ingroup tui_internal
 * @brief Internal feature flags and configuration switches.
 */

#ifndef POSTOFFICE_TUI_INTERNAL_CONFIG_H
#define POSTOFFICE_TUI_INTERNAL_CONFIG_H

/* Enable integration with perf ring buffers for events */
#ifndef PO_TUI_WITH_PERF_EVENTS
#define PO_TUI_WITH_PERF_EVENTS 1
#endif

/* Enable ncurses terminal backend */
#ifndef PO_TUI_WITH_NCURSES
#define PO_TUI_WITH_NCURSES 1
#endif

/* Maximum sizes for internal buffers (sane defaults) */
#ifndef PO_TUI_MAX_DRAW_CMDS
#define PO_TUI_MAX_DRAW_CMDS 8192
#endif

#endif /* POSTOFFICE_TUI_INTERNAL_CONFIG_H */
