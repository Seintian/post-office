/**
 * @file timerwheel.c
 * @ingroup tui_util
 * @brief HOW: Simple timer scheduler implementation.
 */

#include "timerwheel.h"

#include <stdlib.h>

typedef struct tw_node {
    struct tw_node *next;
    double when_ms;
    double interval_ms;
    timer_cb cb;
    void *ud;
    struct timer_id id;
} tw_node;

struct timerwheel {
    tw_node *head;
    unsigned long long seq;
};

timerwheel *timerwheel_create(void) {
    return (timerwheel *)calloc(1, sizeof(timerwheel));
}
void timerwheel_destroy(timerwheel *tw) {
    if (!tw)
        return;
    tw_node *n = tw->head;
    while (n) {
        tw_node *nx = n->next;
        free(n);
        n = nx;
    }
    free(tw);
}

static void list_insert(timerwheel *tw, tw_node *node) {
    if (!tw->head || node->when_ms < tw->head->when_ms) {
        node->next = tw->head;
        tw->head = node;
        return;
    }
    tw_node *p = tw->head;
    while (p->next && p->next->when_ms <= node->when_ms)
        p = p->next;
    node->next = p->next;
    p->next = node;
}

static struct timer_id make_id(timerwheel *tw) {
    struct timer_id id = {++tw->seq};
    return id;
}

timer_id timerwheel_add(timerwheel *tw, double now_ms, double delay_ms, timer_cb cb, void *ud) {
    tw_node *n = (tw_node *)calloc(1, sizeof(*n));
    if (!n)
        return (struct timer_id){0};
    n->when_ms = now_ms + delay_ms;
    n->interval_ms = 0.0;
    n->cb = cb;
    n->ud = ud;
    n->id = make_id(tw);
    list_insert(tw, n);
    return n->id;
}
timer_id timerwheel_add_interval(timerwheel *tw, double now_ms, double interval_ms, timer_cb cb,
                                 void *ud) {
    tw_node *n = (tw_node *)calloc(1, sizeof(*n));
    if (!n)
        return (struct timer_id){0};
    n->when_ms = now_ms + interval_ms;
    n->interval_ms = interval_ms;
    n->cb = cb;
    n->ud = ud;
    n->id = make_id(tw);
    list_insert(tw, n);
    return n->id;
}

void timerwheel_cancel(timerwheel *tw, timer_id id) {
    tw_node **pp = &tw->head;
    while (*pp) {
        tw_node *n = *pp;
        if (n->id.id == id.id) {
            *pp = n->next;
            free(n);
            return;
        }
        pp = &(*pp)->next;
    }
}

void timerwheel_tick(timerwheel *tw, double now_ms) {
    while (tw->head && tw->head->when_ms <= now_ms) {
        tw_node *n = tw->head;
        tw->head = n->next;
        if (n->cb)
            n->cb(n->ud);
        if (n->interval_ms > 0.0) {
            n->when_ms = now_ms + n->interval_ms;
            n->next = NULL;
            list_insert(tw, n);
        } else {
            free(n);
        }
    }
}
/**
 * @file timerwheel.c
 * @ingroup tui_util
 * @brief HOW: O(1) bucketed timer wheel for deferred actions.
 */

// Implementation will be added in subsequent milestones.
