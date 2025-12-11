# Simulation lifecycle skeleton

This short note describes the skeleton lifecycle implementation added to the codebase.

Files added/modified:

- `src/core/simulation/director/process/process.h` — Added a minimal `po_process` descriptor, lifecycle states and inline helpers (`po_process_init`, `po_process_is_active`, etc.). This is intentionally minimal and header-only to let Director code reference process descriptors without needing a new translation unit.

- `src/core/simulation/worker/runtime/worker_loop.c` — Added a small worker runtime skeleton providing `worker_init()` and `worker_run()` with signal-driven shutdown. Placeholder comments indicate where IPC, task polling, metrics and cleanup should be implemented.

- `src/core/simulation/user/runtime/user_loop.c` — Added a small user runtime skeleton providing `user_init()` and `user_run()` with signal-driven shutdown. Placeholder comments indicate where request generation and IPC should be implemented.

- `src/core/simulation/director/ctrl_bridge/bridge_mainloop.h` — Declared a minimal bridge mainloop API: `bridge_mainloop_init()`, `bridge_mainloop_run()` and `bridge_mainloop_stop()`.

Notes and next steps:

- The implementations are skeletons meant to provide a clear lifecycle until death (init -> run loop -> graceful shutdown). They should be expanded with real IPC interactions, metrics, state updates, and director orchestration.

- Suggested follow-ups:
  - Wire `worker_run()` / `user_run()` into the respective `main()` entry points (if desired) so the process executables actually call into these run loops.
  - Implement `bridge_mainloop.c` using the declared API, integrating with the Director loop.
  - Add tests that validate the signal-driven shutdown behavior and ensure resources are released.
