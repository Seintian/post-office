/**
 * @file ui_loop.c
 * @ingroup tui_core
 * @brief HOW: Engine main loop orchestration implementation.
 *
 * Responsibilities:
 *  - Poll input via `input_poll_step()` (may block up to budget slice).
 *  - Drain event ring buffer and dispatch to active screen/widgets.
 *  - Advance timers (delta clock) and schedule actions (timer wheel).
 *  - Mark layout dirty on state/theme/size changes.
 *  - On render: build draw batches, push to renderer buffer, present.
 *
 * Performance:
 *  - Event queue: `perf` ring buffer for O(1) push/pop.
 *  - Draw: `perf` batcher aggregates runs of same style.
 *  - Memory: zero-copy pools for large surfaces to avoid realloc.
 */

// Implementation will be added in subsequent milestones.
