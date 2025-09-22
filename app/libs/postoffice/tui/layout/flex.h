/**
 * @file flex.h
 * @ingroup tui_layout
 * @brief Flex layout constraints (direction, alignment, grow/shrink, gaps).
 */

#ifndef POSTOFFICE_TUI_LAYOUT_FLEX_H
#define POSTOFFICE_TUI_LAYOUT_FLEX_H

typedef struct po_tui_flex po_tui_flex; /* opaque (internal) */

typedef enum flex_direction { FLEX_ROW, FLEX_COLUMN } flex_direction;
typedef enum flex_wrap { FLEX_NOWRAP, FLEX_WRAP } flex_wrap;
typedef enum flex_justify {
    FLEX_JUSTIFY_START,
    FLEX_JUSTIFY_CENTER,
    FLEX_JUSTIFY_END,
    FLEX_JUSTIFY_BETWEEN,
    FLEX_JUSTIFY_AROUND
} flex_justify;
typedef enum flex_align {
    FLEX_ALIGN_START,
    FLEX_ALIGN_CENTER,
    FLEX_ALIGN_END,
    FLEX_ALIGN_STRETCH
} flex_align;

typedef struct flex_config {
    flex_direction direction;
    flex_wrap wrap;
    flex_justify justify;
    flex_align align;
    int gap_main;  /* spaces between items along main axis */
    int gap_cross; /* spaces between rows/cols on wrap */
} flex_config;

po_tui_flex *flex_create(const flex_config *cfg);
void flex_destroy(po_tui_flex *fx);
void flex_set_config(po_tui_flex *fx, const flex_config *cfg);

#endif /* POSTOFFICE_TUI_LAYOUT_FLEX_H */
