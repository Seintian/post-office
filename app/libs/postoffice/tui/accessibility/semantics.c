/**
 * @file semantics.c
 * @ingroup tui_accessibility
 * @brief HOW: Simple semantics node tree and properties storage.
 */

#include "accessibility/semantics.h"

#include <stdlib.h>
#include <string.h>

struct semantics_node {
    struct semantics_node *parent;
    struct semantics_node *first_child;
    struct semantics_node *next_sibling;
    tui_role role;
    unsigned state;
    struct {
        int x, y, w, h;
    } rect;
    char *name;
    char *desc;
};

static void free_children(struct semantics_node *n) {
    struct semantics_node *c = n->first_child;
    while (c) {
        struct semantics_node *nx = c->next_sibling;
        semantics_node_destroy(c);
        c = nx;
    }
}

semantics_node *semantics_node_create(tui_role role) {
    semantics_node *n = (semantics_node *)calloc(1, sizeof(*n));
    if (!n)
        return NULL;
    n->role = role;
    return n;
}
void semantics_node_destroy(semantics_node *n) {
    if (!n)
        return;
    free_children(n);
    free(n->name);
    free(n->desc);
    free(n);
}

void semantics_node_set_parent(semantics_node *child, semantics_node *parent) {
    if (!child)
        return;
    child->parent = parent;
}
semantics_node *semantics_node_get_parent(const semantics_node *n) {
    return n ? n->parent : NULL;
}
void semantics_node_append_child(semantics_node *parent, semantics_node *child) {
    if (!parent || !child)
        return;
    child->parent = parent;
    child->next_sibling = NULL;
    if (!parent->first_child) {
        parent->first_child = child;
    } else {
        struct semantics_node *c = parent->first_child;
        while (c->next_sibling)
            c = c->next_sibling;
        c->next_sibling = child;
    }
}

void semantics_node_set_name(semantics_node *n, const char *utf8_name) {
    if (!n)
        return;
    free(n->name);
    n->name = utf8_name ? strdup(utf8_name) : NULL;
}
void semantics_node_set_description(semantics_node *n, const char *utf8_desc) {
    if (!n)
        return;
    free(n->desc);
    n->desc = utf8_desc ? strdup(utf8_desc) : NULL;
}
void semantics_node_set_role(semantics_node *n, tui_role role) {
    if (!n)
        return;
    n->role = role;
}
void semantics_node_set_state(semantics_node *n, unsigned state_bits, unsigned mask) {
    if (!n)
        return;
    n->state = (n->state & ~mask) | (state_bits & mask);
}
void semantics_node_set_bounds(semantics_node *n, tui_a11y_rect rect) {
    if (!n)
        return;
    n->rect.x = rect.x;
    n->rect.y = rect.y;
    n->rect.w = rect.w;
    n->rect.h = rect.h;
}

void semantics_announce(semantics_node *n, const char *utf8, int politeness) {
    (void)n;
    (void)utf8;
    (void)politeness; /* TODO: post UI event */
}
