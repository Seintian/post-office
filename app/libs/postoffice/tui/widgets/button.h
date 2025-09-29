/**
 * @file button.h
 * @ingroup tui_widgets
 * @brief Activatable button widget with focus/press states.
 */

#ifndef POSTOFFICE_TUI_WIDGETS_BUTTON_H
#define POSTOFFICE_TUI_WIDGETS_BUTTON_H

/** Opaque handle to a button widget instance. */
typedef struct po_tui_button po_tui_button; /* opaque (internal) */

/**
 * @brief Click callback signature for a button.
 *
 * The callback is invoked when the button is activated (e.g., key press or
 * mouse click) while the button has focus.
 *
 * @param ud User data pointer provided during configuration.
 */
typedef void (*button_on_click)(void *ud);

/**
 * @struct button_config
 * @brief Construction parameters for a button widget.
 */
typedef struct button_config {
    /** Label rendered inside the button (UTF-8, copied or referenced internally). */
    const char *text;
    /** Callback invoked on activation (may be NULL for no action). */
    button_on_click on_click;
    /** User data pointer passed to the on_click callback. */
    void *on_click_ud;
} button_config;

/**
 * @brief Create a new button widget.
 *
 * @param cfg Button configuration (must not be NULL).
 * @return Newly created button instance, or NULL on allocation failure.
 */
po_tui_button *button_create(const button_config *cfg);

/**
 * @brief Destroy a button widget and release its resources.
 *
 * Safe to call with NULL (no-op).
 *
 * @param btn Button instance to destroy (may be NULL).
 */
void button_destroy(po_tui_button *btn);

/**
 * @brief Set or update the button label.
 *
 * @param btn  Target button (must not be NULL).
 * @param utf8 New UTF‑8 label text (owned by caller, copied internally).
 */
void button_set_text(po_tui_button *btn, const char *utf8);

/**
 * @brief Configure the click callback for a button.
 *
 * @param btn Target button (must not be NULL).
 * @param cb  Callback function (may be NULL to clear).
 * @param ud  User data pointer passed to the callback.
 */
void button_set_on_click(po_tui_button *btn, button_on_click cb, void *ud);

#endif /* POSTOFFICE_TUI_WIDGETS_BUTTON_H */
