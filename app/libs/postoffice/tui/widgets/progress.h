/**
 * @file progress.h
 * @ingroup tui_widgets
 * @brief Progress bar widget supporting determinate and indeterminate modes.
 */

#ifndef POSTOFFICE_TUI_WIDGETS_PROGRESS_H
#define POSTOFFICE_TUI_WIDGETS_PROGRESS_H

/** Opaque handle to a progress bar widget instance. */
typedef struct po_tui_progress po_tui_progress; /* opaque (internal) */

/**
 * @enum progress_mode
 * @brief Rendering mode for the progress widget.
 */
typedef enum progress_mode { PROGRESS_DETERMINATE, PROGRESS_INDETERMINATE } progress_mode;

/**
 * @struct progress_config
 * @brief Construction parameters for a progress bar.
 */
typedef struct progress_config {
    /** Initial mode; determinate shows value/max, indeterminate animates. */
    progress_mode mode;
    /** Current progress value (ignored if indeterminate). */
    int value;
    /** Maximum value that represents 100% (must be > 0 for determinate). */
    int max;
} progress_config;

/**
 * @brief Create a new progress bar widget.
 * @param cfg Configuration parameters (must not be NULL).
 * @return New instance or NULL on allocation failure.
 */
po_tui_progress *progress_create(const progress_config *cfg);

/**
 * @brief Destroy a progress bar widget.
 * @param p Instance to destroy (may be NULL).
 */
void progress_destroy(po_tui_progress *p);

/**
 * @brief Set the current determinate progress value.
 * @param p     Target progress bar (must not be NULL).
 * @param value New value within [0, max]. Values outside the range are clamped.
 */
void progress_set_value(po_tui_progress *p, int value);

/**
 * @brief Set the maximum determinate progress value (100%% threshold).
 * @param p   Target progress bar (must not be NULL).
 * @param max New maximum; must be > 0 for determinate mode.
 */
void progress_set_max(po_tui_progress *p, int max);

/**
 * @brief Change the rendering mode.
 * @param p    Target progress bar (must not be NULL).
 * @param mode New mode (determinate/indeterminate).
 */
void progress_set_mode(po_tui_progress *p, progress_mode mode);

#endif /* POSTOFFICE_TUI_WIDGETS_PROGRESS_H */
