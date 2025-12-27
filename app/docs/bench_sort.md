# Sort Benchmark Tool (`bench_sort`)

This tool provides a comparative analysis between the standard C library's `qsort` and the custom `po_sort` (FluxSort variant) implementation.

## Usage

```bash
make tools
./bin/bench_sort
```

## Performance Characteristics

### Optimization Sensitivity
`po_sort` is a template-heavy implementation that relies heavily on aggressive compiler optimizations to achieve peak performance.

| Build Level | Performance vs `qsort` | Reasons |
| :--- | :--- | :--- |
| **Debug (`-O1`)** | ~0.3x (3x Slower) | Function call overhead, lack of inlining, no auto-vectorization. |
| **Debug + ASAN** | ~0.3x (Even Slower) | Instrumentation overhead on the hot path (fully instrumented `po_sort` vs un-instrumented `libc qsort`). |
| **Release (`-O3`)** | **5x - 12x (Faster)** | Full inlining, loop unrolling, SIMD utilization, and induction variable optimizations. |

### Adaptive Speedups
FluxSort is highly adaptive and outperforms `qsort` even in unoptimized builds for specific data distributions:
- **Sorted Data**: Up to **13x faster**.
- **Reverse Sorted**: Up to **8x faster**.
- **Few Unique Elements**: Up to **5x faster**.

## Profiling Analysis (-O1 Meltdown)

Direct investigation using `perf` revealed that in `-O1` builds:
1. **ASAN Bias**: ASAN instrumentation adds a constant factor of ~2.5x to every comparison and swap in `po_sort`.
2. **Induction Failure**: The compiler fails to simplify the generic `base + index * size` pointer arithmetic in the partitioning loop.
3. **Synchronization Overhead**: The parallel dispatching mechanism (`threadpool`, `waitgroup`) adds significant latency for $N < 1M$ when not optimized.

## Release Build Benchmarks (N=5M Random)

| Metric | qsort (`libc`) | po_sort (`-O3`) |
| :--- | :--- | :--- |
| **Time (s)** | 0.8226s | **0.1483s** |
| **IPC** | ~0.8 | **~1.8** |
| **Speedup** | 1.0x | **5.55x** |
