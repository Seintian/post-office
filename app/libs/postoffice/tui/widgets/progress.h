/**
 * @file progress.h
 * @ingroup tui_widgets
 * @brief WHAT: Progress bar widget (determinate/indeterminate).
 */

#ifndef POSTOFFICE_TUI_WIDGETS_PROGRESS_H
#define POSTOFFICE_TUI_WIDGETS_PROGRESS_H

typedef struct po_tui_progress po_tui_progress; /* opaque (internal) */

typedef enum progress_mode { PROGRESS_DETERMINATE, PROGRESS_INDETERMINATE } progress_mode;
typedef struct progress_config {
    progress_mode mode;
    int value;
    int max;
} progress_config;

po_tui_progress *progress_create(const progress_config *cfg);
void progress_destroy(po_tui_progress *p);
void progress_set_value(po_tui_progress *p, int value);
void progress_set_max(po_tui_progress *p, int max);
void progress_set_mode(po_tui_progress *p, progress_mode mode);

#endif /* POSTOFFICE_TUI_WIDGETS_PROGRESS_H */
