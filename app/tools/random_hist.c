#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "random/random.h"

int main(int argc, char **argv) {
    // Optional deterministic seed via argv[1]; otherwise auto-seed.
    if (argc > 1) {
        uint64_t seed = strtoull(argv[1], NULL, 10);
        po_rand_seed(seed);
    } else
        po_rand_seed_auto();

    int counts[101] = {0}; // use indices 1..100

    for (int i = 0; i < 1000; ++i) {
        long v = po_rand_range_i64(1, 100);
        counts[v]++;
    }

    // Print CSV: value,count
    for (int v = 1; v <= 100; ++v)
        printf("%d,%d\n", v, counts[v]);

    return 0;
}
