#include <ctype.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "random/random.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -n, --samples N    Number of samples (default: 10000)\n");
    fprintf(stderr, "  -m, --min N        Minimum value (default: 1)\n");
    fprintf(stderr, "  -M, --max N        Maximum value (default: 100)\n");
    fprintf(stderr, "  -w, --width N      Histogram width (default: 50)\n");
    fprintf(stderr, "  -S, --seed N       Random seed (default: auto)\n");
    fprintf(stderr, "  -h, --help         Show this help message\n");
}

int main(int argc, char **argv) {
    int samples = 10000;
    int min_val = 1;
    int max_val = 100;
    int bar_width = 50;
    uint64_t seed = 0;
    int seed_provided = 0;

    static struct option long_options[] = {
        {"samples", required_argument, 0, 'n'},
        {"min", required_argument, 0, 'm'},
        {"max", required_argument, 0, 'M'},
        {"width", required_argument, 0, 'w'},
        {"seed", required_argument, 0, 'S'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "n:m:M:w:S:h", long_options, &long_index)) != -1) {
        switch (opt) {
            case 'n': samples = atoi(optarg); break;
            case 'm': min_val = atoi(optarg); break;
            case 'M': max_val = atoi(optarg); break;
            case 'w': bar_width = atoi(optarg); break;
            case 'S': 
                seed = strtoull(optarg, NULL, 10);
                seed_provided = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (samples <= 0) samples = 1;
    if (min_val >= max_val) {
        fprintf(stderr, "Error: min (%d) must be less than max (%d)\n", min_val, max_val);
        return 1;
    }
    if (bar_width <= 0) bar_width = 10;
    // Cap simple array size for stack allocation or use dynamic allocation. 
    // The previous code had `int counts[101]`. 
    // If user requests range [1, 1000], stack array will crash if too small.
    // We should allocate dynamically based on range.
    
    int range_size = max_val - min_val + 1;
    int *counts = calloc((size_t)range_size, sizeof(int));
    if (!counts) {
        perror("calloc");
        return 1;
    }

    if (seed_provided) {
        po_rand_seed(seed);
    } else {
        po_rand_seed_auto();
    }

    int max_freq = 0;
    long long sum = 0; // Use long long to avoid overflow

    for (int i = 0; i < samples; ++i) {
        long v = po_rand_range_i64(min_val, max_val);
        // Map value to index 0..range_size-1
        int idx = (int)(v - min_val);
        if (idx >= 0 && idx < range_size) {
            counts[idx]++;
            if (counts[idx] > max_freq) max_freq = counts[idx];
            sum += v;
        }
    }

    printf("\nRandom Distribution Histogram\n");
    printf("=============================\n");
    printf("Samples: %d\n", samples);
    printf("Range:   [%d, %d]\n", min_val, max_val);
    printf("Mean:    %.2f\n", (double)sum / samples);
    printf("----------------------------------------------------------------------\n");
    printf("  Val  |   Freq   | Graph\n");
    printf("-------|----------|---------------------------------------------------\n");

    for (int v = min_val; v <= max_val; ++v) {
        int idx = v - min_val;
        int len = 0;
        if (max_freq > 0) {
            // Avoid overflow in calculation: cast to long long
            len = (int)(( (long long)counts[idx] * bar_width ) / max_freq);
        }
        
        printf(" %5d | %8d | ", v, counts[idx]);
        for (int k = 0; k < len; ++k) putchar('#');
        putchar('\n');
    }
    printf("----------------------------------------------------------------------\n");

    free(counts);
    return 0;
}
