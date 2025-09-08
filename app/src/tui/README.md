# TUI Framework (Generic Layer)

This directory contains the reusable, simulation-agnostic Text User Interface framework.

It intentionally excludes any application-specific concepts (screens, components, adapters, IPC, overlays) that belong to the Post Office simulation domain.

## Layering

- core/: context, loop orchestration, registry, events, state, command dispatch, popup infra
- layout/: layout algorithms (flex/grid/stack) and geometry helpers
- render/: renderer abstraction, buffers, terminal backend, draw batching & theming
- input/: input polling, key mapping, command line editing frontend hooks
- theme/: palette, symbols, style configuration
- widgets/: primitive building blocks (labels, text, progress, table core, etc.)
- util/: small generic utilities (ring view, debounce, time helpers, string builder)
- internal/: private headers (visibility, macros, internal config) not installed publicly

## What lives elsewhere

Application-bound code has been moved under `app/src/main/`:

- screens/
- components/
- adapters/ (logstore, director, perf, entities, config)
- diagnostics/ overlays
- ipc/ (simulation messaging & decode)

## Public API

The umbrella public header will be `ui.h` (to be populated) exposing:

- tui_init / tui_shutdown
- tui_tick / tui_render
- tui_post_event / tui_post_command
- tui_register_screen / tui_set_active_screen

## Build Integration

The Makefile auto-discovers `*.c` sources under this directory as part of `TUI_SRC`.

## Next Steps

1. Define and implement `ui_context` and minimal event loop.
2. Implement renderer backends and diffing.
3. Implement at least one widget fully with test coverage.
4. Provide a thin adapter layer for the application to plug in its screens.

Keep this layer free from direct dependencies on simulation-specific headers.
