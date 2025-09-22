/**
 * @file semantics.h
 * @ingroup tui_accessibility
 * @brief Accessibility semantics tree, roles, states, and announcement API.
 *
 * WHAT:
 *  - Defines roles, states, properties (name/description), and geometry for
 *    accessibility nodes attached to widgets.
 *  - Exposes a small tree API to parent/child nodes and to emit announcements.
 *
 * HOW (see `semantics.c`):
 *  - Nodes are lightweight and can be pooled.
 *  - Announcements are posted as UI events for screen-reader adapters.
 */

#ifndef POSTOFFICE_TUI_ACCESSIBILITY_SEMANTICS_H
#define POSTOFFICE_TUI_ACCESSIBILITY_SEMANTICS_H

typedef enum tui_role {
    TUI_ROLE_NONE = 0,
    TUI_ROLE_LABEL,
    TUI_ROLE_BUTTON,
    TUI_ROLE_CHECKBOX,
    TUI_ROLE_RADIO,
    TUI_ROLE_PROGRESS,
    TUI_ROLE_SLIDER,
    TUI_ROLE_LIST,
    TUI_ROLE_LIST_ITEM,
    TUI_ROLE_TABLE,
    TUI_ROLE_TABLE_ROW,
    TUI_ROLE_TABLE_CELL,
    TUI_ROLE_TEXT,
    TUI_ROLE_TEXT_INPUT,
    TUI_ROLE_GROUP,
    TUI_ROLE_DIALOG
} tui_role;

/** Bitmask of state flags for accessibility nodes */
typedef enum tui_a11y_state {
    TUI_A11Y_FOCUSABLE = 1u << 0,
    TUI_A11Y_FOCUSED = 1u << 1,
    TUI_A11Y_DISABLED = 1u << 2,
    TUI_A11Y_CHECKED = 1u << 3,
    TUI_A11Y_SELECTED = 1u << 4,
    TUI_A11Y_EXPANDED = 1u << 5,
    TUI_A11Y_COLLAPSED = 1u << 6,
} tui_a11y_state;

/** Rectangle in grid-cell coordinates */
typedef struct tui_a11y_rect {
    int x, y, w, h;
} tui_a11y_rect;

typedef struct semantics_node semantics_node; /* opaque */

/** Create/destroy a semantics node for a widget */
semantics_node *semantics_node_create(tui_role role);
void semantics_node_destroy(semantics_node *n);

/** Parenting (tree) */
void semantics_node_set_parent(semantics_node *child, semantics_node *parent);
semantics_node *semantics_node_get_parent(const semantics_node *n);
void semantics_node_append_child(semantics_node *parent, semantics_node *child);

/** Properties */
void semantics_node_set_name(semantics_node *n, const char *utf8_name);
void semantics_node_set_description(semantics_node *n, const char *utf8_desc);
void semantics_node_set_role(semantics_node *n, tui_role role);
void semantics_node_set_state(semantics_node *n, unsigned state_bits, unsigned mask);
void semantics_node_set_bounds(semantics_node *n, tui_a11y_rect rect);

/**
 * @brief Emit an announcement for screen readers (live region).
 * @param n node (may be NULL for global context)
 * @param utf8 text to announce
 * @param politeness 0=off, 1=polite, 2=assertive
 */
void semantics_announce(semantics_node *n, const char *utf8, int politeness);

#endif /* POSTOFFICE_TUI_ACCESSIBILITY_SEMANTICS_H */
