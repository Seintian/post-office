
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>

#include "postoffice/sort/sort.h"
#include "postoffice/random/random.h"

// Simple timer
static double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int int_cmp(const void *a, const void *b) {
    const int *ia = (const int *)a;
    const int *ib = (const int *)b;
    return (*ia > *ib) - (*ia < *ib);
}

typedef enum {
    MODE_RANDOM,
    MODE_SORTED,
    MODE_REVERSE,
    MODE_RANDOM_FEW_UNIQUE,
    MODE_MOORE, // already largely sorted with some random swaps
    MODE_SAWTOOTH, // Many alternating runs
    MODE_PERTURBED // Sorted with 1% swaps
} data_mode_t;

static void fill_data(int *data, size_t n, data_mode_t mode) {
    if (mode == MODE_MOORE) {
        // 4 independent sorted runs with noise
        size_t q = n / 4;
        int sub_idx = 0;
        int run = 0;
        for (size_t i = 0; i < n; i++) {
            data[i] = sub_idx++;
            // Add noise (1%)
            if (po_rand_range_i64(0, 99) == 0) data[i] = (int)po_rand_u32();
            if ((size_t)sub_idx >= q) { sub_idx = 0; run++; }
        }
        return;
    }
    if (mode == MODE_SAWTOOTH) {
        // Runs of length 1024, alternating ascending/descending
        size_t run_len = 1024;
        int val = 0;
        bool asc = true;
        for (size_t i = 0; i < n; i++) {
            data[i] = val;
            val += (asc ? 1 : -1);
            if ((i + 1) % run_len == 0) {
                asc = !asc;
                val = asc ? 0 : 1023;
            }
        }
        return;
    }
    if (mode == MODE_PERTURBED) {
        // Sorted with random swaps
        for (size_t i = 0; i < n; i++) data[i] = (int)i;
        size_t swaps = n / 100; // 1%
        for (size_t i = 0; i < swaps; i++) {
            size_t a = po_rand_range_i64(0, n - 1);
            size_t b = po_rand_range_i64(0, n - 1);
            int tmp = data[a];
            data[a] = data[b];
            data[b] = tmp;
        }
        return;
    }

    for (size_t i = 0; i < n; i++) {
        switch (mode) {
            case MODE_RANDOM: 
                data[i] = (int)po_rand_u32(); 
                break;
            case MODE_SORTED: 
                data[i] = (int)i; 
                break;
            case MODE_REVERSE: 
                data[i] = (int)(n - i); 
                break;
            case MODE_RANDOM_FEW_UNIQUE: 
                data[i] = (int)po_rand_range_i64(0, 9); 
                break;
            default: break;
        }
    }
}

static void run_bench(const char *name, size_t n, data_mode_t mode) {
    int *data_qsort = malloc(n * sizeof(int));
    int *data_po = malloc(n * sizeof(int));

    fill_data(data_qsort, n, mode);
    memcpy(data_po, data_qsort, n * sizeof(int));

    printf("Benchmarking %s (N=%zu): ", name, n);
    fflush(stdout);

    // QSORT
    double start = get_time_sec();
    qsort(data_qsort, n, sizeof(int), int_cmp);
    double end = get_time_sec();
    double qsort_time = end - start;
    printf("qsort: %.4fs ", qsort_time);

    // PO_SORT
    start = get_time_sec();
    po_sort(data_po, n, sizeof(int), int_cmp);
    end = get_time_sec();
    double po_time = end - start;
    printf("| po_sort: %.4fs ", po_time);
    fflush(stdout);

    // Speedup
    printf("| (x%.2f speedup)\n", qsort_time / po_time);

    // Verify
    for (size_t i = 1; i < n; i++) {
        if (data_po[i-1] > data_po[i]) {
            printf("ERROR: po_sort failed to sort at index %zu\n", i);
            break;
        }
    }

    free(data_qsort);
    free(data_po);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    size_t sizes[] = { 10000, 100000, 1000000, 5000000 };
    po_rand_seed_auto();
    po_sort_init();

    printf("=== Sort Benchmark: po_sort (FluxSort Parallel) vs qsort ===\n\n");

    for (size_t i = 0; i < 4; i++) {
        size_t N = sizes[i];
        printf("\n--- Size: %zu ---\n", N);
        // Run benchmarks (Isolating Moore for perf analysis)
        run_bench("Random", N, MODE_RANDOM);
        run_bench("Sorted", N, MODE_SORTED);
        run_bench("Reverse", N, MODE_REVERSE);
        run_bench("Few Unique", N, MODE_RANDOM_FEW_UNIQUE);
        run_bench("Moore", N, MODE_MOORE);
        run_bench("Sawtooth", N, MODE_SAWTOOTH);
        run_bench("Perturbed", N, MODE_PERTURBED);
    }

    return 0;
}
