/**
 * @file text_layout.h
 * @ingroup tui_text
 * @brief Text layout API: wrapping, alignment, and measurement.
 */

#ifndef POSTOFFICE_TUI_TEXT_TEXT_LAYOUT_H
#define POSTOFFICE_TUI_TEXT_TEXT_LAYOUT_H

struct text_layout; /* opaque */

typedef enum text_wrap { TEXT_NOWRAP, TEXT_WRAP_WORD, TEXT_WRAP_CHAR } text_wrap;
typedef enum text_align { TEXT_ALIGN_START, TEXT_ALIGN_CENTER, TEXT_ALIGN_END } text_align;

typedef struct text_layout_config {
    int max_width; /* <=0 unlimited */
    text_wrap wrap;
    text_align align;
} text_layout_config;

struct text_layout *text_layout_create(void);
void text_layout_destroy(struct text_layout *tl);
void text_layout_set_config(struct text_layout *tl, const text_layout_config *cfg);
int text_layout_measure(const struct text_layout *tl, const char *utf8, int *out_w, int *out_h);

#endif /* POSTOFFICE_TUI_TEXT_TEXT_LAYOUT_H */
