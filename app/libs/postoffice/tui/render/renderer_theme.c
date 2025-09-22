/**
 * @file renderer_theme.c
 * @ingroup tui_render
 * @brief HOW: Mapping logical colors/styles to backend attributes.
 */

#include "render/renderer_theme.h"

#include <stdlib.h>

struct renderer_theme {
    int dummy;
};

struct renderer_theme *renderer_theme_create(void) {
    return (struct renderer_theme *)calloc(1, sizeof(struct renderer_theme));
}
void renderer_theme_destroy(struct renderer_theme *th) {
    free(th);
}

unsigned renderer_theme_attr_color(struct renderer_theme *th, unsigned fg, unsigned bg) {
    (void)th;
    (void)fg;
    (void)bg;
    return 0;
}
unsigned renderer_theme_attr_style(struct renderer_theme *th, unsigned attrs) {
    (void)th;
    return attrs;
}
