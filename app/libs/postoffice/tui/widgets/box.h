/**
 * @file box.h
 * @ingroup tui_widgets
 * @brief Box container with optional border and title.
 */

#ifndef POSTOFFICE_TUI_WIDGETS_BOX_H
#define POSTOFFICE_TUI_WIDGETS_BOX_H

typedef struct po_tui_box po_tui_box; /* opaque (internal) */

typedef struct box_config {
    const char *title;
    unsigned border_mask; /* TLBR bits */
} box_config;

po_tui_box *box_create(const box_config *cfg);
void box_destroy(po_tui_box *bx);
void box_set_title(po_tui_box *bx, const char *utf8);
void box_set_border(po_tui_box *bx, unsigned mask);

#endif /* POSTOFFICE_TUI_WIDGETS_BOX_H */
