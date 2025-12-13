#include <ctype.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "random/random.h"

typedef struct {
    int id;
    int samples;
    int min_val;
    int max_val;
    int range_size;
    uint64_t seed_base;
    int use_seed;
    int *local_counts;
    long long local_sum;
} ThreadInfo;

static void *worker_thread(void *arg) {
    ThreadInfo *info = (ThreadInfo *)arg;

    // Allocate local histogram
    info->local_counts = calloc((size_t)info->range_size, sizeof(int));
    if (!info->local_counts) {
        perror("calloc thread");
        pthread_exit(NULL);
    }

    // Seed logic
    if (info->use_seed) {
        // Deterministic seeding per thread: base + id
        // A simple addition is enough for xoshiro seeding to diverge, 
        // but mixing it slightly is safer to avoid correlation in very simple generators.
        // For xoshiro, just ensuring different states is enough.
        po_rand_seed(info->seed_base + (uint64_t)info->id);
    } else {
        po_rand_seed_auto();
    }

    info->local_sum = 0;
    for (int i = 0; i < info->samples; ++i) {
        long v = po_rand_range_i64(info->min_val, info->max_val);
        int idx = (int)(v - info->min_val);
        if (idx >= 0 && idx < info->range_size) {
            info->local_counts[idx]++;
            info->local_sum += v;
        }
    }

    pthread_exit(NULL);
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -n, --samples N    Number of samples (default: 10000)\n");
    fprintf(stderr, "  -m, --min N        Minimum value (default: 1)\n");
    fprintf(stderr, "  -M, --max N        Maximum value (default: 100)\n");
    fprintf(stderr, "  -w, --width N      Histogram width (default: 50)\n");
    fprintf(stderr, "  -S, --seed N       Random seed (default: auto)\n");
    fprintf(stderr, "  -t, --threads N    Number of threads (default: 1)\n");
    fprintf(stderr, "  -h, --help         Show this help message\n");
}

int main(int argc, char **argv) {
    int samples = 10000;
    int min_val = 1;
    int max_val = 100;
    int bar_width = 50;
    uint64_t seed = 0;
    int seed_provided = 0;
    int num_threads = 1;

    static struct option long_options[] = {
        {"samples", required_argument, 0, 'n'},
        {"min", required_argument, 0, 'm'},
        {"max", required_argument, 0, 'M'},
        {"width", required_argument, 0, 'w'},
        {"seed", required_argument, 0, 'S'},
        {"threads", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "n:m:M:w:S:t:h", long_options, &long_index)) != -1) {
        switch (opt) {
            case 'n': samples = atoi(optarg); break;
            case 'm': min_val = atoi(optarg); break;
            case 'M': max_val = atoi(optarg); break;
            case 'w': bar_width = atoi(optarg); break;
            case 't': num_threads = atoi(optarg); break;
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
    if (num_threads < 1) num_threads = 1;

    int range_size = max_val - min_val + 1;
    
    // Allocate global counts
    int *counts = calloc((size_t)range_size, sizeof(int));
    if (!counts) {
        perror("calloc");
        return 1;
    }

    // Setup threads
    pthread_t *threads = malloc((size_t)num_threads * sizeof(pthread_t));
    ThreadInfo *t_info = malloc((size_t)num_threads * sizeof(ThreadInfo));

    if (!threads || !t_info) {
        perror("malloc threads");
        free(counts);
        free(threads);
        free(t_info);
        return 1;
    }

    int samples_per_thread = samples / num_threads;
    int remainder = samples % num_threads;

    for (int i = 0; i < num_threads; ++i) {
        t_info[i].id = i;
        // Distribute remainder samples among first few threads
        t_info[i].samples = samples_per_thread + (i < remainder ? 1 : 0);
        t_info[i].min_val = min_val;
        t_info[i].max_val = max_val;
        t_info[i].range_size = range_size;
        t_info[i].seed_base = seed;
        t_info[i].use_seed = seed_provided;
        t_info[i].local_counts = NULL;
        t_info[i].local_sum = 0;

        if (pthread_create(&threads[i], NULL, worker_thread, &t_info[i]) != 0) {
            perror("pthread_create");
            // In a real app we might cleanup here, for now just exit/crash
            exit(1);
        }
    }

    // Join and aggregate
    long long total_sum = 0;
    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
        if (t_info[i].local_counts) {
            for (int k = 0; k < range_size; ++k) {
                counts[k] += t_info[i].local_counts[k];
            }
            free(t_info[i].local_counts);
        }
        total_sum += t_info[i].local_sum;
    }

    free(threads);
    free(t_info);

    // Calculate max freq for scaling
    int max_freq = 0;
    for (int i = 0; i < range_size; ++i) {
        if (counts[i] > max_freq) max_freq = counts[i];
    }

    printf("\nRandom Distribution Histogram\n");
    printf("=============================\n");
    printf("Samples: %d\n", samples);
    printf("Threads: %d\n", num_threads);
    printf("Range:   [%d, %d]\n", min_val, max_val);
    printf("Mean:    %.2f\n", (double)total_sum / samples);
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
