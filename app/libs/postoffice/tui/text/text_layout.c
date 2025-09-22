/**
 * @file text_layout.c
 * @ingroup tui_text
 * @brief HOW: Text wrapping and width computation (basic implementation).
 */

#include "text/text_layout.h"

#include <stdlib.h>
#include <string.h>

struct text_layout {
    struct text_layout_config cfg;
};

struct text_layout *text_layout_create(void) {
    return (struct text_layout *)calloc(1, sizeof(struct text_layout));
}
void text_layout_destroy(struct text_layout *tl) {
    free(tl);
}
void text_layout_set_config(struct text_layout *tl, const text_layout_config *cfg) {
    if (!tl)
        return;
    if (cfg)
        tl->cfg = *cfg;
}

int text_layout_measure(const struct text_layout *tl, const char *utf8, int *out_w, int *out_h) {
    (void)tl;
    if (!utf8)
        return -1;
    int maxw = 0, lines = 1, curw = 0;
    const char *p = utf8;
    while (*p) {
        if (*p == '\n') {
            if (curw > maxw)
                maxw = curw;
            curw = 0;
            lines++;
            p++;
            continue;
        }
        curw++;
        p++;
    }
    if (curw > maxw)
        maxw = curw;
    if (out_w)
        *out_w = maxw;
    if (out_h)
        *out_h = lines;
    return 0;
}
