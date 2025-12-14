/**
 * @file false_sharing_bench.c
 * @brief Benchmark demonstrating the performance impact of false sharing
 *        and the benefits of cache line padding.
 *
 * This tool creates two scenarios:
 * 1. WITHOUT padding: Multiple threads contend on adjacent data (false sharing)
 * 2. WITH padding: Each thread's data is isolated on separate cache lines
 *
 * The benchmark measures the dramatic performance difference and visualizes
 * the results with clear ASCII charts.
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>

// ============================================================================
// Configuration
// ============================================================================

#define NUM_THREADS 2
#define ITERATIONS_PER_THREAD 10000000
#define MAX_CACHE_LINE_SIZE 128  // Support up to 128-byte cache lines

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Counter WITHOUT cache line padding - will cause false sharing
 */
typedef struct {
    atomic_uint_fast64_t value;
} counter_unpadded_t;

/**
 * Counter WITH cache line padding - prevents false sharing
 * Padding is dynamically sized based on detected cache line size
 */
typedef struct {
    atomic_uint_fast64_t value;
    char _pad[MAX_CACHE_LINE_SIZE - sizeof(atomic_uint_fast64_t)];
} counter_padded_t __attribute__((aligned(MAX_CACHE_LINE_SIZE)));

/**
 * Thread arguments for benchmark
 */
typedef struct {
    int thread_id;
    void *counter;
    uint64_t iterations;
    int pin_to_core;
} thread_args_t;

/**
 * Benchmark results
 */
typedef struct {
    double elapsed_seconds;
    uint64_t total_operations;
    double ops_per_second;
    double ns_per_op;
} bench_result_t;

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get current time in nanoseconds
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * Pin thread to specific CPU core
 */
static void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET((size_t)core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

/**
 * Detect cache line size from system
 * Returns detected size, or conservative default if detection fails
 */
static size_t detect_cache_line_size(void) {
    long size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

    if (size > 0 && size <= MAX_CACHE_LINE_SIZE) {
        return (size_t)size;
    }

    // Conservative default: 64 bytes (most common on x86-64, ARM64)
    // This works correctly on systems with smaller cache lines (wastes memory)
    // and provides partial benefit on systems with larger cache lines
    return 64;
}

// ============================================================================
// Benchmark Workers
// ============================================================================

/**
 * Worker thread for UNPADDED counters (false sharing scenario)
 */
static void *worker_unpadded(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    counter_unpadded_t *counters = (counter_unpadded_t *)args->counter;

    if (args->pin_to_core >= 0) {
        pin_to_core(args->pin_to_core);
    }

    // Each thread increments its own counter
    for (uint64_t i = 0; i < args->iterations; i++) {
        atomic_fetch_add_explicit(&counters[args->thread_id].value, 1, 
                                   memory_order_relaxed);
    }

    return NULL;
}

/**
 * Worker thread for PADDED counters (no false sharing)
 */
static void *worker_padded(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    counter_padded_t *counters = (counter_padded_t *)args->counter;

    if (args->pin_to_core >= 0) {
        pin_to_core(args->pin_to_core);
    }

    // Each thread increments its own counter
    for (uint64_t i = 0; i < args->iterations; i++) {
        atomic_fetch_add_explicit(&counters[args->thread_id].value, 1, 
                                   memory_order_relaxed);
    }

    return NULL;
}

// ============================================================================
// Benchmark Execution
// ============================================================================

/**
 * Run benchmark with UNPADDED counters
 */
static bench_result_t run_benchmark_unpadded(int num_threads, uint64_t iterations, 
                                              int pin_cores) {
    counter_unpadded_t *counters = calloc((size_t)num_threads, sizeof(counter_unpadded_t));
    pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
    thread_args_t *args = calloc((size_t)num_threads, sizeof(thread_args_t));

    // Initialize counters
    for (int i = 0; i < num_threads; i++) {
        atomic_init(&counters[i].value, 0);
    }

    // Start timing
    uint64_t start = get_time_ns();

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        args[i].counter = counters;
        args[i].iterations = iterations;
        args[i].pin_to_core = pin_cores ? i : -1;
        pthread_create(&threads[i], NULL, worker_unpadded, &args[i]);
    }

    // Wait for completion
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // End timing
    uint64_t end = get_time_ns();

    // Calculate results
    bench_result_t result;
    result.elapsed_seconds = (double)(end - start) / 1e9;
    result.total_operations = (uint64_t)num_threads * iterations;
    result.ops_per_second = (double)result.total_operations / result.elapsed_seconds;
    result.ns_per_op = (double)(end - start) / (double)result.total_operations;

    // Verify correctness
    for (int i = 0; i < num_threads; i++) {
        uint64_t val = atomic_load(&counters[i].value);
        if (val != iterations) {
            fprintf(stderr, "ERROR: Counter %d has value %lu, expected %lu\n", 
                    i, val, iterations);
        }
    }

    free(counters);
    free(threads);
    free(args);

    return result;
}

/**
 * Run benchmark with PADDED counters
 */
static bench_result_t run_benchmark_padded(int num_threads, uint64_t iterations, 
                                            int pin_cores) {
    // Detect actual cache line size for proper alignment
    size_t cacheline_size = detect_cache_line_size();
    
    // Allocate with detected cache line alignment
    counter_padded_t *counters = aligned_alloc(cacheline_size, 
                                                (size_t)num_threads * sizeof(counter_padded_t));
    pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
    thread_args_t *args = calloc((size_t)num_threads, sizeof(thread_args_t));

    // Initialize counters
    for (int i = 0; i < num_threads; i++) {
        atomic_init(&counters[i].value, 0);
    }

    // Start timing
    uint64_t start = get_time_ns();

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        args[i].counter = counters;
        args[i].iterations = iterations;
        args[i].pin_to_core = pin_cores ? i : -1;
        pthread_create(&threads[i], NULL, worker_padded, &args[i]);
    }

    // Wait for completion
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // End timing
    uint64_t end = get_time_ns();

    // Calculate results
    bench_result_t result;
    result.elapsed_seconds = (double)(end - start) / 1e9;
    result.total_operations = (uint64_t)num_threads * iterations;
    result.ops_per_second = (double)result.total_operations / result.elapsed_seconds;
    result.ns_per_op = (double)(end - start) / (double)result.total_operations;

    // Verify correctness
    for (int i = 0; i < num_threads; i++) {
        uint64_t val = atomic_load(&counters[i].value);
        if (val != iterations) {
            fprintf(stderr, "ERROR: Counter %d has value %lu, expected %lu\n", 
                    i, val, iterations);
        }
    }

    free(counters);
    free(threads);
    free(args);

    return result;
}

// ============================================================================
// Visualization
// ============================================================================

/**
 * Print a horizontal bar chart
 */
static void print_bar(const char *label, double value, double max_value, 
                      const char *color __attribute__((unused))) {
    int bar_width = 50;
    int filled = (int)((value / max_value) * bar_width);

    printf("  %-20s [", label);
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("█");
        } else {
            printf("░");
        }
    }
    printf("] %.2f\n", value);
}

/**
 * Print memory layout visualization
 */
static void print_memory_layout(void) {
    size_t cacheline_size = detect_cache_line_size();

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║              MEMORY LAYOUT VISUALIZATION                       ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    printf("WITHOUT PADDING (False Sharing):\n");
    printf("  Cache Line 0 (%zu bytes):\n", cacheline_size);
    printf("  ┌────────┬────────┬────────┬────────┬────────┬────────┬────────┬────────┐\n");
    printf("  │ Cnt[0] │ Cnt[1] │ Cnt[2] │ Cnt[3] │ Cnt[4] │ Cnt[5] │ Cnt[6] │ Cnt[7] │\n");
    printf("  └────────┴────────┴────────┴────────┴────────┴────────┴────────┴────────┘\n");
    printf("     ↑        ↑        ↑        ↑\n");
    printf("   Thread0  Thread1  Thread2  Thread3  ← ALL ON SAME CACHE LINE!\n");
    printf("   When Thread0 writes → invalidates cache for Thread1,2,3\n");
    printf("\n");

    printf("WITH PADDING (No False Sharing):\n");
    size_t padding_bytes = cacheline_size - sizeof(atomic_uint_fast64_t);
    printf("  Cache Line 0: ┌────────┬──────────────────────────────────────────────┐\n");
    printf("                │ Cnt[0] │         padding (%zu bytes)%*s│\n", 
           padding_bytes, (int)(23 - (padding_bytes >= 100 ? 3 : padding_bytes >= 10 ? 2 : 1)), "");
    printf("                └────────┴──────────────────────────────────────────────┘\n");
    printf("                   ↑\n");
    printf("                 Thread0\n");
    printf("\n");
    printf("  Cache Line 1: ┌────────┬──────────────────────────────────────────────┐\n");
    printf("                │ Cnt[1] │         padding (%zu bytes)%*s│\n", 
           padding_bytes, (int)(23 - (padding_bytes >= 100 ? 3 : padding_bytes >= 10 ? 2 : 1)), "");
    printf("                └────────┴──────────────────────────────────────────────┘\n");
    printf("                   ↑\n");
    printf("                 Thread1  ← ISOLATED! No cache line sharing\n");
    printf("\n");
    printf("  ... (Thread2, Thread3 on separate cache lines)\n");
    printf("\n");
}

/**
 * Print comprehensive results
 */
static void print_results(bench_result_t unpadded, bench_result_t padded) {
    double speedup = unpadded.elapsed_seconds / padded.elapsed_seconds;
    double max_time = (unpadded.elapsed_seconds > padded.elapsed_seconds) 
                      ? unpadded.elapsed_seconds : padded.elapsed_seconds;

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           FALSE SHARING BENCHMARK RESULTS                      ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    printf("System Information:\n");
    printf("  Threads:           %d\n", NUM_THREADS);
    printf("  Iterations/thread: %d\n", ITERATIONS_PER_THREAD);
    printf("  Total operations:  %lu\n", unpadded.total_operations);
    printf("  Cache line size:   %zu bytes\n", detect_cache_line_size());
    printf("\n");

    printf("────────────────────────────────────────────────────────────────\n");
    printf("Performance Comparison:\n");
    printf("────────────────────────────────────────────────────────────────\n");
    printf("\n");

    printf("Execution Time (seconds):\n");
    print_bar("WITHOUT padding", unpadded.elapsed_seconds, max_time, "red");
    print_bar("WITH padding", padded.elapsed_seconds, max_time, "green");
    printf("\n");

    printf("Throughput (Million ops/sec):\n");
    double max_ops = (unpadded.ops_per_second > padded.ops_per_second) 
                     ? unpadded.ops_per_second : padded.ops_per_second;
    print_bar("WITHOUT padding", unpadded.ops_per_second / 1e6, max_ops / 1e6, "red");
    print_bar("WITH padding", padded.ops_per_second / 1e6, max_ops / 1e6, "green");
    printf("\n");

    printf("Latency (nanoseconds/op):\n");
    double max_ns = (unpadded.ns_per_op > padded.ns_per_op) 
                    ? unpadded.ns_per_op : padded.ns_per_op;
    print_bar("WITHOUT padding", unpadded.ns_per_op, max_ns, "red");
    print_bar("WITH padding", padded.ns_per_op, max_ns, "green");
    printf("\n");

    printf("────────────────────────────────────────────────────────────────\n");
    printf("Summary:\n");
    printf("────────────────────────────────────────────────────────────────\n");
    printf("\n");
    printf("  SPEEDUP: %.2fx faster with cache line padding!\n", speedup);
    printf("\n");
    printf("  WITHOUT padding: %.2f seconds (%.2f M ops/sec, %.2f ns/op)\n",
           unpadded.elapsed_seconds, unpadded.ops_per_second / 1e6, unpadded.ns_per_op);
    printf("  WITH padding:    %.2f seconds (%.2f M ops/sec, %.2f ns/op)\n",
           padded.elapsed_seconds, padded.ops_per_second / 1e6, padded.ns_per_op);
    printf("\n");

    if (speedup > 2.0) {
        printf("  ✓ SIGNIFICANT IMPROVEMENT: Cache line padding is highly effective!\n");
    } else if (speedup > 1.2) {
        printf("  ✓ MODERATE IMPROVEMENT: Cache line padding provides measurable benefit.\n");
    } else {
        printf("  ⚠ MINIMAL IMPROVEMENT: May indicate low contention or other bottlenecks.\n");
    }
    printf("\n");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[]) {
    int num_threads = NUM_THREADS;
    uint64_t iterations = ITERATIONS_PER_THREAD;
    int pin_cores = 1;  // Pin threads to cores by default

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            num_threads = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            iterations = (uint64_t)atoll(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--no-pin") == 0) {
            pin_cores = 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  --threads N       Number of threads (default: %d)\n", NUM_THREADS);
            printf("  --iterations N    Iterations per thread (default: %d)\n", ITERATIONS_PER_THREAD);
            printf("  --no-pin          Don't pin threads to CPU cores\n");
            printf("  --help            Show this help message\n");
            return 0;
        }
    }

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║        FALSE SHARING BENCHMARK - Cache Line Padding Demo       ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    print_memory_layout();

    printf("Running benchmarks...\n");
    printf("  Phase 1: WITHOUT padding (false sharing scenario)...\n");
    bench_result_t unpadded = run_benchmark_unpadded(num_threads, iterations, pin_cores);

    printf("  Phase 2: WITH padding (isolated cache lines)...\n");
    bench_result_t padded = run_benchmark_padded(num_threads, iterations, pin_cores);

    print_results(unpadded, padded);

    return 0;
}
