# Post Office - Generic TUI Framework Layer

Reusable, simulation‑agnostic terminal UI engine providing layout, event, rendering, theming, and widget infrastructure for the broader application. All domain logic (simulation screens, adapters, IPC, metrics overlays, etc.) intentionally lives outside this layer.

---

## 1. Goals & Non‑Goals

| Category | Goals | Non‑Goals (for now) |
|----------|-------|---------------------|
| Portability | Plain C (C11+) minimal deps, pluggable terminal backend | Direct `ncursesw` coupling in core |
| Architecture | Clear separation: input → events → state → layout → render | Full MVC or retained scene graph |
| Extensibility | Add widgets/layouts/backends without modifying existing code | Dynamic plugin loading |
| Performance | Batching, minimal allocations per frame, O(visible) traversal | GPU acceleration, advanced font shaping |
| Testability | Deterministic event loop & pure layout algorithms | Full golden master visual diffs initially |
| Theming | Palette + symbol indirection + (planned) style cascade | CSS‑complete system |
| Accessibility | Foundational semantics layer placeholder | Screen reader / AT protocol bridge |

---

## 2. Directory Overview

```txt
core/            Engine: context, loop, registry, events, state, popup infra
input/           Keyboard + (placeholder) mouse + command line editing & polling
internal/        Private headers (visibility, macros, internal config, screen internals)
layout/          Layout algorithms (flex, grid, stack) + geometry helpers
render/          Renderer abstraction, batching, buffers, terminal backend stub, theme rendering
theme/           Palette, symbols, theme definition (low level)
style/           (Placeholder) style cascade & resolution planned layer
text/            (Placeholder) text layout / wrapping / wide char shaping
widgets/         Primitive & composite widget implementations
window/          (Placeholder) multi‑window / viewport / tab management
accessibility/   (Placeholder) semantics & role metadata
util/            Generic utilities (debounce, clocks, timer wheel, ring view, string builder)
```

### Added Placeholders (Not Implemented Yet)

`input/mouse/`, `accessibility/`, `text/`, `style/`, `window/` — these exist on disk to stake out architectural seams and keep future work discoverable.

---

## 3. Core Concepts

| Concept | Summary | Notes |
|---------|---------|-------|
| Context (`ui_context`) | Central handle referencing registries, active screen, timing, queues | Singleton per process (passed explicitly) |
| Event System | Unified queue of input + synthetic events | Decouples producers from consumers |
| Commands vs Events | Commands = high‑level intents (e.g. `FOCUS_NEXT`), Events = raw input (`KEY_PRESS`) | Command mapping done in input/keymap layer |
| Layout Pass | Converts widget tree constraints → concrete geometry (flex/grid/stack) | Should be pure / side‑effect free |
| Render Pass | Traverses visible widgets, builds draw batch, flushes via backend | Double buffer strongly recommended |
| Theming | Color + symbol indirection | Style cascade will extend this |
| Timing | Delta clock + timer wheel for deferred operations | Avoid busy waiting; integrate with blocking input poll |

---

## 4. Lifecycle (Planned Public API)

```c
// header: ui.h (to be populated)
bool tui_init(struct tui_config *cfg);
void tui_shutdown(void);

// Main loop integration (app drives):
bool tui_tick(double frame_budget_ms);   // process timers, input, dispatch events
bool tui_render(void);                   // perform layout diff & flush renderer

// Interaction:
bool tui_post_event(const struct tui_event *ev);
bool tui_post_command(enum tui_command cmd, void *payload);

// Screens / views:
struct tui_screen *tui_register_screen(const struct tui_screen_desc *desc);
bool tui_set_active_screen(struct tui_screen *);
```

Lifecycle phases per frame:

1. Input Poll → translate raw input → events → command mapping.
2. Dispatch / State Mutations (popups, focus, widget state updates).
3. Layout calculation (only if dirty).
4. Render batching + diff (compare previous buffer) → backend flush.
5. Timers & deferred actions scheduling.

Dirty flags are used to short‑circuit layout & render when nothing changed (planned optimization).

---

## 5. Threading & Timing Model

Current model: Single threaded deterministic loop. All widget mutations must occur inside the tick phase. Background data sources communicate via external queues → marshalled into TUI events before `tui_tick` returns.

Planned extension: Optional async ingestion buffer (`lock‑free ring`) feeding the event queue (still consumed on main thread).

Timer wheel: O(1) bucketed timers targeting coarse scheduling (animations / refresh). High‑precision sub‑millisecond not required.

---

## 6. Event & Input Architecture

Layers:

```txt
OS / Terminal
   ↓ (raw chars / escape sequences / mouse reports)
input_poll.c → keymap.c → command_line.c → ui_events → ui_commands → ui_state
```

Planned Mouse: `input/mouse/mouse.[ch]` will parse X10 / SGR mouse sequences (if backend supports). For now it is a stub.

Command Mapping Strategy:
    - Raw key (key + modifiers) → lookup → command enum.
    - Unmapped sequences become `TUI_EVENT_KEY_TYPED`.

---

## 7. Layout System

Implemented algorithms (stubs now):
    - Flex: directional, gap, grow/shrink factors.
    - Grid: fixed rows/cols, possible expansion ratios.
    - Stack: z‑ordering / overlay composition (e.g. popups).

All produce a `geometry` description (rects + clipping). Layout must be pure: given the same widget tree & constraints, results identical. Enables caching and potential incremental re‑layout.

### Geometry Invariants

- Coordinates are 0-based, inclusive start, exclusive end.
- No overlapping sibling rects outside Stack or intentional overlay.
- Popups allocated from a higher z-layer via stack engine.

---

## 8. Rendering Pipeline

Stages:

1. Widget traversal builds a `draw_batch` (ordered primitives: cells, spans, box lines, etc.).
2. `renderer_buffer` holds current & previous frame surfaces.
3. Diff algorithm emits minimal terminal update sequence.
4. `renderer_term` backend performs actual writes (future: alternative backends).

Planned optimizations:

- Cell damage tracking (dirty rectangles).
- Deferred color/symbol resolution (apply at flush time).
- Optional glyph cache after text layout implemented.

---

## 9. Theming & Style (Current vs Planned)

Current theme layer: Palette (indexed color roles) + symbolic characters (border, progress fill, etc.).

Planned style cascade (`style/`):

- Selector model: widget type + state flags (focus, disabled, selected).
- Merge order: base theme < widget default < style rules < inline overrides.
- Resolved style struct cached per widget until invalidated.

---

## 10. Text & Rich Layout (Planned)

`text/text_layout.[ch]` will supply:

- Soft wrapping with width constraints.
- Grapheme cluster iteration (Unicode basic; full shaping out of scope initially).
- Wide character cell width handling (East Asian Width heuristic).
- Optional ellipsis for overflow.

---

## 11. Accessibility / Semantics (Planned)

`accessibility/semantics.[ch]` will define:

- Roles (LABEL, BUTTON, TABLE, PROGRESS, LIST_ITEM, etc.).
- State flags (focused, pressed, disabled, busy).
- Tree extraction API for external tooling / diagnostics.

No screen reader integration planned short term; aim is introspection & potential future export.

---

## 12. Window / Multi‑Screen (Planned)

`window/window_manager.[ch]` placeholder for logical viewports / tab sets:

- Manage multiple root widget trees.
- Off‑screen render + composition.
- Potential tiling or tab bar integration.

Initial milestone may keep a single active root; manager provides future expansion path.

---

## 13. Widgets

Current set (stubs): box, button, label, progress, sidebar_list, table_core, text.

Planned taxonomy:

| Category | Examples | Notes |
|----------|----------|-------|
| Containers | box, stack, flex container | Provide layout constraints |
| Display | label, text, progress | Read‑only state |
| Interactive | button, list item selection | Requires focus & command routing |
| Data | table_core | Pagination / virtual scrolling future |
| Overlay | popup (in core) | Renders on stack layer |

Widget Authoring Guidelines:

1. Keep rendering pure (no side effects; produce primitives only).
2. State changes occur only via event/command handlers.
3. Expose minimal public construction API; internal mutation hidden.

---

## 14. Extensibility Patterns

### Adding a Widget

1. Create `widgets/foo.c/.h`.
2. Define a descriptor struct (creation params + vtable).
3. Register in `ui_registry` (or auto-register via macro pattern later).
4. Add to style defaults once cascade system exists.

### Adding a Layout Algorithm

1. Implement pure function: `layout_<algo>(widget_tree, constraints, out_geometry)`.
2. Extend `layout_engine` dispatch table.
3. Add enum value + factory.

### Adding a Renderer Backend

1. Create `render/renderer_<name>.c`.
2. Implement backend interface (init, shutdown, present, query size, write primitives).
3. Select via config or runtime probe.

---

## 15. Error Handling & Logging

- This layer defers to the global application logging module (no duplicate logger).
- Functions return `bool` / error code; avoid `abort()` inside framework.
- Provide `tui_last_error()` (planned) returning thread‑local static string.

---

## 16. Memory & Performance Guidelines

| Area | Guidance |
|------|----------|
| Allocation | Preallocate renderer buffers & draw batch vectors; reuse across frames |
| Strings | Use `string_builder` for incremental assembly |
| Timing | Single monotonic clock read per frame; pass down |
| Events | Use ring or fixed queue; drop oldest on overflow (configurable) |
| Layout | Cache results; invalidate on size/theme/state change only |
| Rendering | Diff at cell granularity; coalesce runs with same style |

---

## 17. Build & Integration

The Makefile (outside this directory) glob‑collects `*.c`. Adding new modules normally requires only creating files; if selective inclusion is later required, introduce an explicit `TUI_SRC` list.

Recommended future segregation: export only public headers (e.g. install list) while keeping `internal/` excluded.

---

## 18. Testing Strategy (Planned)

| Layer | Approach |
|-------|----------|
| Layout | Pure function golden tests (rect assertions) |
| Events | Simulated input sequences → state assertions |
| Widgets | Snapshot logical representation (not terminal escape output) |
| Renderer | Cell matrix diff tests (no ANSI required) |
| Timing | Deterministic fake clock injection |

Test harness can compile without terminal dependencies. Consider separate `tui_test_support.c`.

---

## 19. Roadmap

| Phase | Focus | Exit Criteria |
|-------|-------|---------------|
| P1 | Minimal loop + keyboard + one layout + label/button widget | Render static & interactive screen |
| P2 | Flex & grid + batch renderer diff + theme | Styleable multi‑section screen |
| P3 | Popup + progress + table core skeleton | Modal interactions stable |
| P4 | Mouse support + text layout (wrap + width) | Mixed input & multiline text |
| P5 | Style cascade + focus navigation | Visual state changes via rules |
| P6 | Semantics tree export + window manager v1 | Multi‑root management |

---

## 20. Versioning & Stability

Internal semantics may change freely until first tagged release (e.g. `tui-0.1.0`). After that, maintain backward compatibility for public functions in `ui.h` with semantic versioning.

---

## 21. Contribution Guidelines (Internal)

1. Prefer small, incremental PRs (one subsystem per change).
2. Document new public symbols in `ui.h` comment blocks.
3. Keep internal headers self‑contained; avoid circular includes.
4. Use feature flags / config structs instead of compile‑time macros where feasible.
5. Update this README when adding major architectural pieces.

---

## 22. Known Gaps (Tracked by Placeholders)

- `mouse`: no parsing yet.
- `style`: cascade engine missing.
- `text_layout`: wrapping + width classification not implemented.
- `semantics`: no role assignment / export.
- `window_manager`: single root only.

---

## 23. License

This framework is part of the Post Office project and inherits the repository root license (`LICENSE`).

---

## 24. Quick Start (Future Example - Not Implemented Yet)

```c
#include "ui.h"

int main(void) {
    struct tui_config cfg = {0};
    if (!tui_init(&cfg))
        return 1;

    while (/* app running */) {
        tui_tick(16.0);   // ~60fps budget
        tui_render();
    }

    tui_shutdown();
    return 0;
}
```

---

## 25. Change Log (Manual - Update When Stabilized)

| Date | Change |
|------|--------|
| 2025-09-08 | Added placeholder modules & exhaustive README rewrite |

---

End of TUI framework specification.
