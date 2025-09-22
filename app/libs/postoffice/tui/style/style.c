/**
 * @file style.c
 * @ingroup tui_style
 * @brief HOW: Style cascade and resolution (basic).
 */

#include "style/style.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct rule {
    struct rule *next;
    style_selector_type sel_type;
    char *sel_value;
    style_props props;
} rule;

struct style_context {
    rule *head;
};

struct style_context *style_context_create(void) {
    return (struct style_context *)calloc(1, sizeof(struct style_context));
}
void style_context_destroy(struct style_context *sc) {
    if (!sc)
        return;
    rule *r = sc->head;
    while (r) {
        rule *n = r->next;
        free(r->sel_value);
        free(r);
        r = n;
    }
    free(sc);
}

bool style_add_rule(struct style_context *sc, style_selector_type sel_type, const char *sel_value,
                    const style_props *props) {
    if (!sc || !props)
        return false;
    rule *r = (rule *)calloc(1, sizeof(*r));
    if (!r)
        return false;
    r->sel_type = sel_type;
    r->props = *props;
    r->sel_value = sel_value ? strdup(sel_value) : NULL;
    r->next = sc->head;
    sc->head = r;
    return true;
}

bool style_resolve(const struct style_context *sc, const char *widget_type,
                   const char *const *classes, int class_count, unsigned state_bits,
                   style_props *out) {
    (void)classes;
    (void)class_count;
    (void)state_bits;
    if (!sc || !out)
        return false;
    memset(out, 0, sizeof(*out));
    /* naive: apply last matching type rule */
    for (rule *r = sc->head; r; r = r->next) {
        if (r->sel_type == SEL_WIDGET_TYPE && r->sel_value && widget_type &&
            strcmp(r->sel_value, widget_type) == 0) {
            *out = r->props;
        }
    }
    return true;
}
