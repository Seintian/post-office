/**
 * @file layout_engine.c
 * @ingroup tui_layout
 * @brief HOW: Dispatch to specific layout algorithms (flex/grid/stack).
 *
 * Purity: Given the same constraints and widget tree, results are deterministic.
 *
 * Performance:
 *  - Avoid allocations; reuse geometry arrays from zero-copy pools.
 *  - Cache results and invalidate via dirty flags.
 */

// Implementation will be added in subsequent milestones.
