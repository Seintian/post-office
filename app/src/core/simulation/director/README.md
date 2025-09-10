# Director Module Architecture

This document maps the current `director/` submodules to the official project specification (PDF "Progetto SO 2024/25 - Ufficio delle Poste"). It explains what goes where (minimal version vs. complete version) and the responsibilities / boundaries of each submodule.

## 0. High‑Level Goals

The Director process orchestrates the whole simulation:

- Initializes configuration & shared resources.
- Spawns the other processes (ticket issuer, operators, users).
- Drives the simulation clock across days.
- Collects & publishes statistics (daily + cumulative + per service).
- Determines termination (timeout / explode threshold).
- (Complete version) Handles dynamic user injection, multi‑request users, CSV export, live control bridge.

Design principles:

- Block instead of busy waiting (spec requires avoiding active wait).
- Deterministic daily phase ordering (init → open → serve → close → report → advance day).
- Minimize shared mutable state surface; confine to `state/` and atomic counters in `telemetry/`.
- Keep extension points explicit (bridge, telemetry exporters) so the *minimal* path remains small.

## 1. Current Directory Layout

```sh
config/       # Load & validate simulation parameters
ctrl_bridge/  # (Optional) External control & UI events bridge (was `bridge/`)
director.c    # Entry + top-level orchestration glue
process/      # Process lifecycle management abstractions
runtime/      # Simulation clock + day loop + termination checks
schedule/     # Task scheduler & resource assignment helpers
state/        # Domain model & state store (sportelli, operators, users)
telemetry/    # Statistics, health & metrics export plumbing
utils/        # Low-level lock / queue / id primitives (may be pruned later)
```

## 2. Mapping to Specification Sections

| Spec Section | Requirement (Summary) | Submodule(s) (Minimal) | Submodule(s) (Complete) |
|--------------|-----------------------|-------------------------|--------------------------|
| 5.1 Director | Start processes, create resources, stats print per day | `director.c`, `process/`, `runtime/`, `state/`, `telemetry/` | + `ctrl_bridge/` (live control) |
| 5.1 Stats    | Maintain totals & daily metrics | `telemetry/` | + CSV exporter (inside `telemetry/` or new `telemetry/csv_export.c`) |
| 5.2 Ticket Issuer | Provide tickets to users | (Implemented outside here; director coordinates via `process/` + shared queues definitions in `state/`) | Same |
| 5.3 Sportelli | Daily assignment & resources | `state/` (sportello structs), `schedule/` (assignment task) | Same |
| 5.4 Operator | Operator process lifecycle, pauses, reassignment | External operator process code; coordination via `state/` & events; director gating via `runtime/` | Same + advanced telemetry |
| 5.5 User     | Users choose service, queue & wait | External user process code; queue + counters in `state/` | Multi‑request logic tracked in `state/` |
| 5.6 Termination | timeout / explode threshold | `runtime/` (check functions), `telemetry/` (counters) | Same + more termination causes if extended |
| 6 Complete   | Multi‑request sequences | `state/` (per-user request list) | Same |
| 6 Complete   | Dynamic new users (N_NEW_USERS) | `ctrl_bridge/` (command) → `process/` (spawn) → `state/` update | Same |
| 6 Complete   | CSV export of stats | (Optional) new `telemetry/csv_export.c` | Same |

## 3. Submodule Responsibilities & Public Interfaces

### 3.1 `director.c`

Single translation unit that wires everything together. Provides (internal naming suggestion):

```c
int director_main(int argc, char **argv);
static bool director_init(...);
static void director_run_loop(...);
static void director_shutdown(...);
```

Invokes: `config/` → `state/` init → `process/` spawn sequence → wait for child ready barrier → enters `runtime/` loop. On termination prints final stats and root cause.

### 3.2 `config/`

Parses INI / environment variables. Delivers an immutable `struct director_config`:

```c
struct director_config {
    uint32_t sim_duration_days; // SIM_DURATION
    uint32_t n_worker_seats;    // NOF_WORKER_SEATS
    uint32_t n_workers;         // NOF_WORKERS
    uint32_t n_users;           // NOF_USERS
    uint32_t max_pauses;        // NOF_PAUSE
    uint64_t minute_ns_scale;   // N_NANO_SECS
    uint32_t explode_threshold; // EXPLODE_THRESHOLD
    // Ranges for P_SERV probabilities
    double p_serv_min;
    double p_serv_max;
    // Future: CSV path, dynamic inject pipe path, etc.
};
```

Validation: ensure non-zero, consistency (e.g., seats ≤ workers). Exported API returns const pointer; no mutation after initialization.

### 3.3 `process/`

Encapsulates fork/exec of other processes and synchronization handshake. Provides:

```c
int proc_spawn_ticket_issuer(...);
int proc_spawn_workers(size_t count, ...);
int proc_spawn_users(size_t count, ...);
int proc_wait_all_initialized(...); // barrier using pipe/semaphore/shared mem flag
```

Abstracts error handling & retry; returns PIDs list for tracking.

### 3.4 `runtime/`

Owns the day loop and the simulation clock:

```c
void runtime_start(const struct director_config*, struct sim_context*);
bool runtime_step_day(struct sim_context*); // returns false when termination reached
enum sim_termination_cause runtime_check_termination(...);
```

Calculates simulated minute progression via sleep / nanosleep scaled by configuration OR triggers events to children via IPC (NOT busy waiting). End-of-day triggers stats snapshot & resource reassignment.

### 3.5 `schedule/`

Lightweight scheduler with a task queue (if truly needed). Minimal variant may only expose:

```c
void schedule_assign_sportelli(struct sim_context*);
```

If advanced tasks (timed callbacks) are *not* needed yet, keep this stubby and avoid complexity. If a task queue is retained:

- `task_queue.c` holds a simple FIFO or min-heap (if time-ordered tasks appear later).
- `scheduler.c` processes tasks at day boundaries or on demand.

### 3.6 `state/`

Holds canonical in-memory model of simulation resources and dynamic counters that are per-day vs cumulative:

```c
struct service_class_stats { uint64_t users_served; uint64_t services_dropped; uint64_t wait_time_ns_total; uint64_t exec_time_ns_total; };
struct director_stats { ... };
struct sportello { enum service_kind kind; pid_t operator_pid; bool open; };
struct sim_context { struct director_config cfg; struct director_stats stats; struct sportello *sportelli; size_t sportelli_count; /* queues, arrays of users, etc. */ };
```

API segmentation:

```c
void state_init(struct sim_context*, const struct director_config*);
void state_new_day(struct sim_context*);
void state_record_service(...);
void state_finalize_day(struct sim_context*);
```

Supports atomic increments if updated by children via shared memory (or funnel updates through messages).

### 3.7 `telemetry/`

- Aggregates stats (pull from `state/` or receive deltas via IPC).
- Provides formatted daily + final report printing.
- Offers optional exporter hooks:

```c
typedef void (*stats_export_cb)(const struct director_stats*, void* user);
int telemetry_register_exporter(stats_export_cb, void* user);
```

Complete version adds CSV writer (periodic or end-of-run): `telemetry/csv_export.c`.
Health monitoring (e.g., operator liveness, queue saturation) can produce warnings; keep simple for minimal.

### 3.8 `ctrl_bridge/`

Abstraction boundary for *external control / monitoring* (e.g., TUI or an admin tool):

- Broadcasts structured events (JSON lines or binary frames) about day transitions, stats snapshots, termination cause.
- Accepts control messages (e.g., add users, force terminate, dump stats now).

If not used yet it can be compiled empty with no-op stubs to avoid cluttering director core.

### 3.9 `utils/`

Currently includes low-level primitives:

- `atomic_queue.c`
- `backoff.c`
- `spinlock.c`
- `id_allocator.c`

Usage policy:

- Use only if **blocking OS primitives** (semaphores, pipes) are insufficient for required latency.
- Each primitive should have a unit test; otherwise consider removing before final submission (simplicity favored in oral discussion).

## 4. Minimal vs Complete Feature Allocation

| Feature | Minimal Implementation | Complete Extension Path |
|---------|------------------------|-------------------------|
| Ticket distribution | Single queue + blocking wait | Per-service queues or dynamic reprioritization |
| Stats printing | End-of-day & final print | CSV export + live streaming via `ctrl_bridge/` |
| User requests | One service ticket/day/user | Batched sequential multi-service list (tracked per user) |
| New users injection | Not supported (static set) | Command via `ctrl_bridge/` spawns & registers users |
| Termination causes | Timeout & explode threshold | Additional (manual abort, deadlock detection) |
| Scheduling | Simple day loop | Timed tasks / operator shift constraints |

## 5. Initialization & Day Loop Sequence

1. Load config (`config/`).
2. Initialize state (`state_init`).
3. Spawn ticket issuer, workers, users (`process/`).
4. Wait for ready barrier (`process/`).
5. Enter runtime loop (`runtime/`):
   - Start day: assign sportelli (`schedule_assign_sportelli`).
   - Signal children: day start.
   - Sleep/advance simulated minutes; children serve users (blocking IPC).
   - End day: `state_finalize_day` → telemetry snapshot → print report.
   - Check termination (`runtime_check_termination`).
6. On termination: final stats export & controlled shutdown of children.

## 6. Concurrency & IPC Strategy (Recommended Minimal)

- Shared memory region: global counters + sportello table (read-mostly by director, write by director; workers only need queue endpoints).
- Ticket request channel: POSIX message queue or pipe (users → ticket issuer); issuer assigns incremental ticket numbers.
- Service completion events: pipe or message queue from operators to director (optional if only end-of-day aggregate needed — then operators update shared counters directly with atomic ops).
- Synchronization primitives: semaphores for queue counts or use blocking `mq_receive`. Avoid spinlocks unless profiling justifies.

## 7. Testing Strategy

| Aspect | Test Type | Notes |
|--------|-----------|-------|
| Config parsing | Unit | Invalid/missing fields, range bounds |
| State transitions | Unit | New day reset vs cumulative retention |
| Termination logic | Unit | Explode threshold triggers, timeout at exact boundary |
| Scheduling | Unit/Integration | Operator-seat assignment invariants |
| Telemetry formatting | Unit | Stable output lines for regression checks |
| Runtime loop | Integration (short sim) | 1 day, tiny timing constants |
| Ctrl bridge (optional) | Unit/Mock | NOP when disabled |

## 8. Future Simplifications / Refactors

- Collapse `runtime/`, `schedule/`, `state/` into fewer files if oral defense time is tight.
- Remove `utils/` primitives not used by at least one hot path.
- Provide a compile-time flag (e.g., `ENABLE_CTRL_BRIDGE`) to exclude bridge code.
- Add `director_internal.h` for internal structs to reduce header surface.

## 9. Naming & Coding Conventions

- Prefix director-internal symbols with `dir_` or `director_` to ease grepping.
- Keep header exposure minimal; only `director.h` (if created) should be included by other processes.
- All cross-process messages should have explicit versioned struct or record type identifiers.

## 10. Deliverables Impact (Exam Perspective)

This breakdown enables you to argue:

- Clear traceability from spec bullets to modules.
- Conscious choice to avoid premature complexity (task heap, advanced telemetry) until needed.
- Compliance with: modularization, use of IPC, avoidance of busy waiting, stats logging, termination semantics.

---
*Revision:* Initial draft (planning stage). Update as modules gain concrete APIs.
