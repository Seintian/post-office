# Performance & Tuning Guide

This guide enumerates the primary knobs, environment variables, and build flags that influence runtime performance, memory usage, and observability for the Post Office simulation & supporting libraries.

The intent is to give a concise checklist when you need to: (a) scale up users/workers, (b) stress the logging/storage subsystems, or (c) collect detailed latency metrics.

---

## 1. High‑Level Optimization Priorities

| Layer | Goal | Typical Bottlenecks | Primary Levers |
|-------|------|---------------------|----------------|
| Simulation (process orchestration) | Deterministic scaling of day cycles | Excess IPC contention, semaphore churn | Worker seat counts, request probabilities |
| IPC (queues, shm, semaphores) | Low overhead synchronization | Oversized critical sections, busy waiting | Proper semaphore granularity, atomic counters |
| Statistics collection | Accurate + low cost | Excessive atomic ping‑pong | Batched local accumulation (future), atomic fetch/add |
| Logger | High throughput, minimal writer stalls | Ring buffer saturation | Ring capacity, policy (drop vs overwrite), consumer threads |
| Storage (logstore + index) | Predictable append latency | Fsync frequency, small batches | Fsync policy, batch size, ring capacity |
| Networking primitives | Cheap framing/protocol ops | Unnecessary copies | Zero‑copy helpers, pre-sized buffers |
| Perf / Metrics | Visibility with bounded overhead | Over‑instrumentation | Selective metric enable, histogram resolution |
| TUI (planned) | Minimal redraw diff cost | Full surface rewrites | Damage tracking, batching |

---

## 2. Build Modes

| Make Target | Implicit Flags | Notes |
|-------------|---------------|-------|
| `make dev` | Full clean + tests + compile_commands | Use during active iteration |
| `make test` | Test instrumentation enabled | Runs unit/integration tests |
| `make doc` | Doxygen invocation | Documentation only |

Potential future split between `DEBUG` vs `RELEASE` (O2/LTO) builds—currently governed by flags inside the top‑level Makefile (inspect before large benchmarks).

---

## 3. Simulation Parameters (INI)

Defined in configuration files in `app/config/*.ini`.

| Section | Key | Effect | Tuning Guidance |
|---------|-----|--------|-----------------|
| `simulation` | `SIM_DURATION` | Number of simulated “days” | Increase to gather stable long‑run averages |
| `simulation` | `N_NANO_SECS` | Nanoseconds per virtual minute | Lower to accelerate perceived time (stress) |
| `simulation` | `EXPLODE_THRESHOLD` | Max queued users before forced termination | Use small value in pressure tests |
| `users` | `NOF_USERS` | Initial user processes | Primary load generator |
| `users` | `P_SERV_MIN`/`P_SERV_MAX` | Service selection probabilities | Narrow range to bias workloads |
| `users` | `N_REQUESTS` | Max services per user | Increases churn & queue pressure |
| `workers` | `NOF_WORKERS` | Total worker processes | Throughput capacity ceiling |
| `workers` | `NOF_WORKER_SEATS` | Concurrent active workers | Controls parallel issuance; right‑size vs users |
| `workers` | `NOF_PAUSE` | Breaks per worker per day | Raise to simulate lower utilization |
| `users_manager` | `N_NEW_USERS` | Batch size for dynamic injection | Burstiness control |

Heuristic: keep `NOF_WORKER_SEATS` close to (active services * 0.6–0.8) to avoid either starvation or idle workers.

---

## 4. Logger Tuning

| Parameter / Concept | Location | Impact | Notes |
|---------------------|----------|--------|-------|
| Ring Capacity | Logger config struct | Drops/overwrites when too small | Power of two recommended |
| Policy (DROP vs OVERWRITE) | Logger config | Back‑pressure behavior | DROP preserves recent bursts; OVERWRITE preserves earliest writers depending on implementation semantics |
| Consumers (threads) | Logger config | Parallel formatting capacity | Diminishing returns >2 unless CPU bound |
| Log Level | CLI / config | Volume of formatted messages | Use `INFO` or `WARN` in perf tests |
| Syslog Sink | CLI toggle | Extra IO latency | Disable for latency microbenchmarks |

Symptoms & Mitigations:

* Frequent overflow notices → Increase ring capacity OR lower level.
* High CPU in logger threads → Reduce formatting complexity (fewer placeholders).

---

## 5. Storage (Logstore & Index)

| Knob | Effect | Trade‑Off |
|------|--------|-----------|
| `ring_capacity` (append queue) | Enqueue headroom | Memory vs drop probability |
| `batch_size` | Syscall / write coalescing | Too large = higher flush latency |
| Fsync Policy (`NONE`, `EACH_BATCH`, `EVERY_N`, `INTERVAL`) | Durability vs latency | Choose `EVERY_N` or `INTERVAL` normally |
| `fsync_every_n` | Frequency (count-based) | P99 latency spikes if too frequent |
| `fsync_interval_ns` | Time window durability | Requires monotonic clock; coalesces naturally |
| Index Size Hint (initial) | Reduces rehash operations | Over‑alloc wastes RAM |

Benchmark Approach:

1. Warm up with representative workload.
2. Measure steady state append latency / throughput.
3. Adjust batch size first, then fsync strategy.

---

## 6. Networking Primitives

Current usage is mostly local / placeholder. Still, for future remote scenarios:

| Area | Optimization |
|------|--------------|
| Framing Encode/Decode | Reuse buffers, avoid intermediate copies |
| Poller Wait | Tune timeout to balance latency vs CPU wakeups |
| Socket Options | Non‑blocking + TCP_NODELAY (if latency sensitive) |
| Message Size | Favor moderate frames; extremely small frames escalate syscalls |

---

## 7. Metrics & Instrumentation

Over‑instrumentation can distort results. Enable only needed categories. Histograms (if implemented) should use coarse bucket sizing first.

Recommended Pattern:

```c
PO_METRIC_LATENCY_RECORD_BEGIN(ctx);
critical_path();
PO_METRIC_LATENCY_RECORD_END(ctx, "component.op.latency_ns");
```

Disable or compile out metrics layer for max throughput tests (future configurable macro/flag).

---

## 8. Statistics Collection Strategy

Current model writes directly to shared atomic counters. If contention grows:

1. Introduce per‑process local accumulators.
2. Periodically aggregate under a lightweight barrier.
3. Keep global atomics for snapshot only.

---

## 9. Process & IPC Scaling Guidelines

| Scenario | Adjustment | Expected Effect |
|----------|-----------|-----------------|
| Too many queued users (explode) | Increase `NOF_WORKER_SEATS` or lower `NOF_USERS` | Reduces queue depth |
| Idle workers | Increase `NOF_USERS` or decrease pauses | Higher utilization |
| High context switch overhead | Reduce total processes (users/workers) | Lowers scheduler pressure |
| Long tail in service time | Adjust probability distribution | Flattens latency distribution |

---

## 10. Crash & Diagnostics Overhead

During performance runs you may disable verbose tracing:

* Set log level ≥ `WARN`.
* Avoid heavy debug asserts (if any compiled under DEBUG macro).

Crash handler cost is negligible until a fatal signal. Ensure core instrumentation (metrics/logging) does not allocate inside signal handlers.

---

## 11. Planned Future Knobs

| Area | Planned Tuning Dimension |
|------|---------------------------|
| TUI Renderer | Damage region size thresholds |
| Style Cascade | Selector cache eviction policy |
| Text Layout | Grapheme segmentation strategy (fast vs full) |
| Metrics | Histogram bucket auto‑scaling |
| Logger | Optional compression for file sink |

---

## 12. Quick Checklist Before Benchmarking

1. Clean build (consistent flags).
2. Disable non‑essential sinks (syslog, verbose stdout).
3. Fix CPU frequency scaling (performance governor) if allowed.
4. Pin processes/threads (future option) for repeatability.
5. Warm up until steady state (cache, file system page cache).
6. Capture metrics & logs with timestamps.
7. Automate run with script to prevent interactive jitter.

---

## 13. Glossary

| Term | Definition |
|------|------------|
| Seat | Concurrent worker execution slot |
| Batch | Group of items drained from a ring or flush list |
| Fsync | Flush file data + metadata durability syscall |
| Explosion | Forced simulation termination due to backlog threshold |

---

Feedback & additions welcome—expand this guide when new subsystems mature.

