#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#ifndef CLOCK_MONOTONIC_COARSE
#define CLOCK_MONOTONIC_COARSE 6
#endif

int main() {
    struct timespec ts;
    uint64_t start, end;
    int i;
    int count = 10000000;

    // Test Coarse
    clock_gettime(CLOCK_MONOTONIC, &ts);
    start = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    
    for(i=0; i<count; i++) {
        clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &ts);
    end = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    printf("Coarse: %lu ns per call\n", (end - start) / count);

    // Test Normal
    clock_gettime(CLOCK_MONOTONIC, &ts);
    start = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    
    for(i=0; i<count; i++) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &ts);
    end = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    printf("Normal: %lu ns per call\n", (end - start) / count);
    
    return 0;
}
