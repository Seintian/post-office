/**
 * @file geometry.h
 * @ingroup tui_layout
 * @brief Geometry helpers for layout calculations.
 */

#ifndef POSTOFFICE_TUI_LAYOUT_GEOMETRY_H
#define POSTOFFICE_TUI_LAYOUT_GEOMETRY_H

typedef struct tui_rect {
    int x, y, w, h;
} tui_rect;

static inline tui_rect tui_rect_make(int x, int y, int w, int h) {
    return (tui_rect){x, y, w, h};
}

static inline int tui_rect_right(tui_rect r) {
    return r.x + r.w;
}
static inline int tui_rect_bottom(tui_rect r) {
    return r.y + r.h;
}
static inline int tui_rect_area(tui_rect r) {
    return r.w * r.h;
}
static inline int tui_rect_is_empty(tui_rect r) {
    return r.w <= 0 || r.h <= 0;
}
static inline int tui_rect_contains(tui_rect r, int x, int y) {
    return x >= r.x && y >= r.y && x < tui_rect_right(r) && y < tui_rect_bottom(r);
}
static inline tui_rect tui_rect_inset(tui_rect r, int l, int t, int rgt, int b) {
    return (tui_rect){r.x + l, r.y + t, r.w - l - rgt, r.h - t - b};
}

#endif /* POSTOFFICE_TUI_LAYOUT_GEOMETRY_H */
