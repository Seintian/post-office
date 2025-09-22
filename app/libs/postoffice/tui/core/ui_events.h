/**
 * @file ui_events.h
 * @ingroup tui_core
 * @brief Event types and queue interface for the TUI engine.
 *
 * WHAT: Defines the unified event model delivered to the engine.
 *  - Raw input (keys, mouse)
 *  - Window changes (resize)
 *  - Timers and synthetic events (e.g., animation ticks)
 *
 * HOW (see `ui_events.c`):
 *  - Backed by `perf` ring buffers for deterministic FIFO processing.
 *  - Zero-copy for large payloads (text blocks) via `perf` pools.
 */

#ifndef POSTOFFICE_TUI_CORE_UI_EVENTS_H
#define POSTOFFICE_TUI_CORE_UI_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

/** Event category */
typedef enum ui_event_type {
    UI_EVENT_NONE = 0,
    UI_EVENT_KEY,
    UI_EVENT_MOUSE,
    UI_EVENT_RESIZE,
    UI_EVENT_TIMER,
    UI_EVENT_CUSTOM
} ui_event_type;

/** Key event payload (subset; extend as needed) */
typedef struct ui_key_event {
    uint32_t key;  /**< platform-agnostic key code */
    uint16_t mods; /**< modifier bitmask */
    bool pressed;  /**< true=press, false=release */
} ui_key_event;

/** Resize payload */
typedef struct ui_resize_event {
    int width;
    int height;
} ui_resize_event;

/** Generic event */
typedef struct ui_event {
    ui_event_type type;
    union {
        ui_key_event key;
        ui_resize_event resize;
        uint64_t timer_id;
        struct {
            const void *ptr;
            uint32_t size;
        } custom;
    } as;
} ui_event;

struct ui_context;

/**
 * @brief Push an event into the engine queue.
 * @return true on success, false if queue full (event may be dropped).
 */
bool ui_events_post(struct ui_context *ctx, const ui_event *ev);

/**
 * @brief Pop next event if available.
 * @return true if an event was written to out, false if queue empty.
 */
bool ui_events_try_pop(struct ui_context *ctx, ui_event *out);

#endif /* POSTOFFICE_TUI_CORE_UI_EVENTS_H */
