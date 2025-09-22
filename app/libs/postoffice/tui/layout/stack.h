/**
 * @file stack.h
 * @ingroup tui_layout
 * @brief Z-ordered overlay composition layout.
 */

#ifndef POSTOFFICE_TUI_LAYOUT_STACK_H
#define POSTOFFICE_TUI_LAYOUT_STACK_H

typedef struct po_tui_stack po_tui_stack; /* opaque (internal) */

typedef struct stack_config {
    unsigned flags;
} stack_config;

po_tui_stack *stack_create(const stack_config *cfg);
void stack_destroy(po_tui_stack *st);

#endif /* POSTOFFICE_TUI_LAYOUT_STACK_H */
