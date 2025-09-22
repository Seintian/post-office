/**
 * @file action_codes.h
 * @ingroup tui_internal
 * @brief Internal action codes for tracing and diagnostics.
 */

#ifndef POSTOFFICE_TUI_INTERNAL_ACTION_CODES_H
#define POSTOFFICE_TUI_INTERNAL_ACTION_CODES_H

typedef enum tui_action_code {
    ACT_NONE = 0,
    /* input */
    ACT_INPUT_POLL,
    ACT_INPUT_DECODE,
    /* layout */
    ACT_LAYOUT_MEASURE,
    ACT_LAYOUT_ARRANGE,
    /* render */
    ACT_RENDER_BUILD_BATCH,
    ACT_RENDER_FLUSH,
} tui_action_code;

#endif /* POSTOFFICE_TUI_INTERNAL_ACTION_CODES_H */
