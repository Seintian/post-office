/**
 * @file layout.h
 * @brief Public layout constraint helpers (optional sugar over container APIs).
 */

#ifndef POSTOFFICE_TUI_LAYOUT_API_H
#define POSTOFFICE_TUI_LAYOUT_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

/* Generic size constraints */
int po_tui_set_min_size(po_tui_widget *w, int min_w, int min_h);
int po_tui_set_max_size(po_tui_widget *w, int max_w, int max_h);
int po_tui_set_padding(po_tui_widget *w, int top, int right, int bottom, int left);
int po_tui_set_align(po_tui_widget *w, float align_x_0_1, float align_y_0_1);

#ifdef __cplusplus
}
#endif

#endif /* POSTOFFICE_TUI_LAYOUT_API_H */
