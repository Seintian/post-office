/**
 * @file style.h
 * @ingroup tui_style
 * @brief Style cascade API: properties, selectors, and inheritance.
 */

#ifndef POSTOFFICE_TUI_STYLE_STYLE_H
#define POSTOFFICE_TUI_STYLE_STYLE_H

struct style_context; /* opaque */

typedef struct style_props {
    unsigned fg;
    unsigned bg;
    unsigned attrs; /* bold/underline etc */
    int padding_l, padding_t, padding_r, padding_b;
} style_props;

typedef enum style_selector_type {
    SEL_WIDGET_TYPE,
    SEL_CLASS,
    SEL_ID,
    SEL_PSEUDO
} style_selector_type;

struct style_context *style_context_create(void);
void style_context_destroy(struct style_context *sc);

/**
 * Add a rule to the context; specificity increases from type->class->id.
 * Pseudo (e.g., :focused, :disabled) can be matched by the engine.
 */
bool style_add_rule(struct style_context *sc, style_selector_type sel_type, const char *sel_value,
                    const style_props *props);

/** Resolve effective style for a widget path and state bitmask */
bool style_resolve(const struct style_context *sc, const char *widget_type,
                   const char *const *classes, int class_count, unsigned state_bits,
                   style_props *out);

#endif /* POSTOFFICE_TUI_STYLE_STYLE_H */
