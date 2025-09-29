# Post Office Architecture

> A high-level, implementation-aware map of the system: major processes, core subsystems, data & control flows, storage model, performance & observability strategy, and extension seams.

---

## 1. Overview

The Post Office project simulates and instruments a multi-process workload orchestrator. Independent processes (director, workers, users, ticket issuer, users manager, dashboard) communicate over lightweight IPC and shared state to emulate a service with request generation, ticket assignment, and work execution. Supporting libraries provide:

- Networking primitives (non-blocking sockets, polling, framing, protocol serialization)
- Storage (append-only log + LMDB-backed index + in-memory index layer)
- Performance primitives (ring buffer, batcher, zero-copy helpers) and metrics facade
- System introspection (sysinfo snapshot: CPU, caches, RAM, huge pages, filesystem, limits)
- Utilities (configuration parsing, random generation, error helpers, file helpers)
- Logging abstraction and structured output support

Design principles:

1. Clear separation between orchestration (process roles) and reusable subsystems.
2. Fail soft where possible (best-effort probes, metrics no-ops when disabled).
3. Memory ownership explicit: caller either adopts returned buffers or provides storage.
4. Predictable performance: static or amortized allocations, minimal hidden I/O.
5. Instrumentation-first: metrics & perf primitives integrated but removable at compile time.

---

## 2. Process Model

| Process | Responsibility | Key Interactions |
|---------|----------------|------------------|
| Director | Global coordinator: spawns others, monitors liveness, orchestrates shutdown | users_manager, workers, ticket_issuer, dashboard |
| Worker | Executes assigned tasks / workloads derived from tickets | ticket_issuer (indirect), shared config / logging |
| User | Simulates client/request generator; produces load | ticket_issuer (requests), metrics, logging |
| Users Manager | Supervises dynamic user processes, aggregates telemetry | director (reports), users (lifecycle) |
| Ticket Issuer | Allocates unique ticket IDs / sequencing, ensures monotonicity | users, workers, logging |
| Main / Dashboard | Aggregated UI / TUI controller displaying system state | metrics, storage (optional), perf counters |

Lifecycle summary:

1. Director initializes core subsystems (logging, configs, metrics/perf, storage).
2. Spawns ticket issuer, users manager (which spawns users), and workers.
3. Steady-state: users request tickets; workers consume tasks; metrics sampled; optional snapshots taken.
4. Shutdown: director signals children (graceful), drains outstanding tasks, finalizes storage & perf flush.

Shutdown guarantees: best-effort flush of logstore & metrics; late user/worker exits do not block but are logged.

---

## 3. Subsystem Map

```text
+------------------+        +--------------------+        +-----------------+
|  Processes       |        |  Subsystems        |        | External Inputs |
| (multi-proc)     |        | (libs/postoffice)  |        | / System State  |
+------------------+        +--------------------+        +-----------------+
| director         |<-----> | net (socket,       | <----> | OS: sockets,    |
| workers (N)      |        |       poller,      |        | epoll, filesys, |
| users (M)        |        |       framing)     |        | procfs, sysfs   |
| users_manager    |        | storage (logstore, |        | LMDB env files  |
| ticket_issuer    |        |          db_lmdb,  |        |                 |
| dashboard/main   |        |          index)    |        |                 |
+------------------+        | perf (ringbuf,     |        +-----------------+
                            |       batcher)     |
                            | metrics (facade)   |
                            | sysinfo            |
                            | log (logger)       |
                            | utils (argv,       |
                            |        configs,    |
                            |        random...)  |
                            +--------------------+
```

---

## 4. Data & Control Flows

### Ticket Lifecycle

1. User requests a ticket (ID allocation via ticket issuer).
2. Ticket distributed to a worker or queued.
3. Worker processes workload associated with the ticket (optional logging / metrics increments).
4. Result or acknowledgment surfaced to dashboard or ignored based on simulation mode.

### Storage Path

- Writes: append-only logstore (sequential fs writes) → optional indexing (offset,length) recorded in LMDB & in-memory index.
- Reads: index lookup (in-memory → LMDB fallback) → file seek & read.
- Integrity: append-only design avoids partial overwrite corruption; recovery rebuilds index from log tail.

### Metrics & Perf

- Fast-path macros (metrics.h) compile to either perf calls or no-ops.
- perf subsystem collects raw counters/histograms using ring buffers; periodic flush merges and prepares snapshots.
- Export: textual tables (fort), developer logs, or TUI panels.

### Sysinfo Snapshot

- Early startup optional probe. Results cached in `po_sysinfo_t` and displayed or logged for tuning reference.

## 5. Storage Architecture

| Component | Role | Notes |
|-----------|------|-------|
| logstore  | Append-only write path; sequential disk writes | Simplicity + durability trade-off; rebuild index on recovery. |
| index     | In-memory hash mapping key → (offset,length) | Holds only latest value; copy-on-insert semantics. |
| db_lmdb   | Persistent key → metadata store (or secondary index) | Provides crash recovery / warm start. |
| LMDB env  | Underlying B+tree pages | ACID semantics leveraged for metadata only (not primary data payloads). |

Failure model: If index lost → rebuild from log; if log truncated → lost trailing keys only. LMDB corruption treated as fatal (fail fast to avoid silent divergence).

## 6. Networking Layer

| Layer    | Responsibility                                  | Highlights                                                  |
|----------|--------------------------------------------------|-------------------------------------------------------------|
| protocol | Message schema (version, type, flags, length)     | Fixed-size header, host/network conversions.                |
| framing  | Parsing / serialization boundaries               | Prevents partial read misuse.                               |
| socket   | Non-blocking wrapper, error normalization        | Consistent errno mapping.                                   |
| poller   | Event multiplexing (epoll/portable)              | Edge-triggered or level-triggered depending on build.       |

Error semantics: operations return -1 + errno; transient (`EAGAIN`/`EINTR`) handled by callers; fatal I/O logged once.

---

## 7. Performance & Observability

| Aspect | Mechanism | Rationale |
|--------|-----------|-----------|
| Counters/Timers | perf primitives (counters, histograms, timers) | Low overhead, composable. |
| Metrics Facade | macros compile out when disabled | Zero-cost in release builds lacking instrumentation. |
| Batching | ring buffer + batcher combine writes | Reduced syscall / lock frequency. |
| Zero-copy | pre-allocated buffers (zerocopy.c) | Minimize allocations & copies. |
| Sysinfo | one-shot environment probe | Capacity planning & tuning hints. |

Hot paths avoid dynamic allocation inside critical loops (ticket issuance, log append, socket polling).

---

## 8. Randomness & Simulation Load

`random.h` provides deterministic seeding (explicit or automatic) enabling reproducible workloads. Uniform generators (`po_rand_u32/u64`), ranged integers, floating unit interval, and Fisher-Yates shuffle support controlled variability in user behavior and scheduling sequences.

---

## 9. Configuration

INI-based configuration (inih) wrapped by `configs.h`.

- Typed getters with default fallback semantics.
- Ownership: caller owns copies; ephemeral parsing buffers freed immediately.
- Failure strategy: missing optional keys → defaults; critical parse errors → startup abort.

---

## 10. Error Handling Strategy

| Pattern | Usage |
|---------|-------|
| Return codes | 0 success / -1 failure (errno set) across most C APIs. |
| Sentinel values | Negative sizes / zero counts to indicate absence (e.g., sysinfo fields). |
| Scoped errno | Functions avoid clobbering errno after success paths. |
| Fatal logs | Only on unrecoverable invariants (corruption, allocation failure). Guard against log spam. |

---

## 11. Memory & Ownership Conventions

- Callers allocate output structs (e.g., `po_sysinfo_t`, index lookups copy keys internally as documented).
- Storage index keeps its own key copies; caller must free original if dynamically allocated.
- No hidden background threads; perf aggregation may be triggered explicitly (or via lightweight hooks).

---

## 12. Extensibility Seams

| Need | Approach |
|------|----------|
| New process role | Add header in `core/`, document lifecycle & IPC contract, register spawn in director. |
| Alternate persistence | Implement new backend parallel to `db_lmdb.*`; expose adapter in storage init. |
| Additional metrics | Add perf counters + macro wrappers; ensure compile-out branch consistent. |
| New protocol messages | Extend protocol enum + framing logic; bump version if wire incompatibility. |
| Pluggable schedulers | Introduce strategy struct consumed by worker execution loop. |

---

## 13. Build & Tooling

- Monolithic `Makefile` builds libraries, executables, tests, and Doxygen docs (`make doc`).
- Tests organized by subsystem; unity provides the minimal C test harness.
- Coverage artifacts (gcno/gcda) generated when compiled with instrumentation flags.

Suggested future enhancements:

- CI gate for Doxygen warnings.
- Sanitizer build variants (ASan/UBSan) for fuzzing new protocol paths.

---

## 14. Security & Robustness Notes

| Area | Consideration |
|------|---------------|
| Input validation | Protocol header length & version validated by framing layer. |
| Resource limits | sysinfo snapshot aids tuning (fd limits, processes) but enforcement left to OS. |
| Crash recovery | logstore replay + LMDB metadata reconstruction path; no partial write repair. |
| Denial resilience | Non-blocking I/O, bounded buffer sizes reduce blocking scenarios. |

---
---

## 15. Future Work (Backlog Ideas)

- Structured metrics export (JSON or Prometheus exposition format).
- Pluggable storage compression layer.
- Adaptive ticket distribution strategies.
- Live reconfiguration (hot reload of INI with diff application).
- Metrics sampling CLI or gRPC endpoint.

---

## 16. Quick Reference (API Groups)

| Group | Purpose |
|-------|---------|
| `net` | Networking (socket, poller, framing, protocol) |
| `storage` | Logstore + index + persistence wrappers |
| `perf` | Performance primitives (ring buffer, batcher, zero-copy) |
| `metrics` | High-level instrumentation facade |
| `sysinfo` | System capability snapshot |
| `utils` | Misc helpers (configs, random, argv, files, errors) |
| `core` | Process orchestration headers |

---
---

## 17. Reading Order Recommendation

1. `core/director.h` - top-level orchestration.
2. `net/protocol.h` + `net/framing.h` - wire contract.
3. `storage/logstore.h` + `storage/index.h` - persistence model.
4. `perf/perf.h` + `metrics.h` - instrumentation backbone.
5. `sysinfo/sysinfo.h` - environment snapshot semantics.
6. `utils/configs.h` & `utils/random.h` - configuration & workload variability.

---

## 18. Glossary

| Term | Meaning |
|------|---------|
| Ticket | Unique ID representing a unit of simulated work. |
| Logstore | Append-only file storing serialized entries (payloads or events). |
| Index | In-memory+LMDB mapping from key → (offset,length) into logstore. |
| Perf primitives | Low-level counters/timers/histograms capturing runtime metrics. |
| Ring buffer | Lock-minimized single/multi-producer buffer used for batching. |

---
---

## 19. License

This architecture document is part of the project and inherits the project license (GPL-3.0-or-later).
