/**
 * @file ring_view.h
 * @ingroup tui_util
 * @brief Zero-copy ring buffer view utilities for contiguous iteration.
 */

#ifndef POSTOFFICE_TUI_UTIL_RING_VIEW_H
#define POSTOFFICE_TUI_UTIL_RING_VIEW_H

#include <stddef.h>

typedef struct ring_view {
    const char *buf;
    size_t cap;
    size_t head;
    size_t tail;
} ring_view;

static inline void ring_view_init(ring_view *rv, const char *buf, size_t cap) {
    rv->buf = buf;
    rv->cap = cap;
    rv->head = rv->tail = 0;
}

/* Returns a contiguous span [ptr,len] starting at tail; advance with pop */
static inline size_t ring_view_peek(const ring_view *rv, const char **ptr) {
    size_t used = (rv->head + rv->cap - rv->tail) % rv->cap;
    size_t first = rv->cap - rv->tail;
    size_t len = used < first ? used : first;
    *ptr = rv->buf + rv->tail;
    return len;
}

static inline void ring_view_pop(ring_view *rv, size_t n) {
    rv->tail = (rv->tail + n) % rv->cap;
}

#endif /* POSTOFFICE_TUI_UTIL_RING_VIEW_H */
