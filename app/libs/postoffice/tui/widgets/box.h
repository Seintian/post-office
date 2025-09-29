/**
 * @file box.h
 * @ingroup tui_widgets
 * @brief Box container widget that draws an optional border and title.
 *
 * A box is a non-interactive container used to visually group other widgets or
 * to frame sections. It can render themed borders on selectable edges and an
 * optional title along the top edge.
 *
 * Border mask uses TLBR bits: top=1, left=2, bottom=4, right=8 (combinable).
 */

#ifndef POSTOFFICE_TUI_WIDGETS_BOX_H
#define POSTOFFICE_TUI_WIDGETS_BOX_H

/** Opaque handle to a box widget instance. */
typedef struct po_tui_box po_tui_box; /* opaque (internal) */

/**
 * @struct box_config
 * @brief Construction parameters for a box widget.
 */
typedef struct box_config {
    /** Optional UTF-8 title drawn on the top border (NULL to disable). */
    const char *title;
    /** TLBR bitmask: top=1, left=2, bottom=4, right=8. */
    unsigned border_mask;
} box_config;

/**
 * @brief Create a new box widget.
 *
 * @param cfg Configuration parameters; if NULL, a borderless, untitled box is created.
 * @return Newly created box instance; NULL on allocation failure.
 */
po_tui_box *box_create(const box_config *cfg);

/**
 * @brief Destroy a box widget and release its resources.
 * @param bx Box instance to destroy (may be NULL).
 */
void box_destroy(po_tui_box *bx);

/**
 * @brief Set or update the title text (UTF-8).
 * @param bx   Target box instance (must not be NULL).
 * @param utf8 New title or NULL to disable the title.
 */
void box_set_title(po_tui_box *bx, const char *utf8);

/**
 * @brief Change which edges draw a border (TLBR mask).
 * @param bx   Target box instance (must not be NULL).
 * @param mask Bitmask: top=1, left=2, bottom=4, right=8. 0 disables all edges.
 */
void box_set_border(po_tui_box *bx, unsigned mask);

#endif /* POSTOFFICE_TUI_WIDGETS_BOX_H */
