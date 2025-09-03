#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "utils/random.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Simple xoshiro256** PRNG (good quality, small state)
// Public domain reference: http://prng.di.unimi.it/

static uint64_t s_state[4] = {0};

static inline uint64_t rotl(const uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

static uint64_t xoshiro256ss(void) {
    const uint64_t result = rotl(s_state[1] * 5, 7) * 9;

    const uint64_t t = s_state[1] << 17;

    s_state[2] ^= s_state[0];
    s_state[3] ^= s_state[1];
    s_state[1] ^= s_state[2];
    s_state[0] ^= s_state[3];

    s_state[2] ^= t;
    s_state[3] = rotl(s_state[3], 45);

    return result;
}

static int seeded(void) {
    const uint64_t any = s_state[0] | s_state[1] | s_state[2] | s_state[3];
    return any != 0 ? 1 : 0;
}

static void seed_from_bytes(const uint8_t *buf, size_t n) {
    // If insufficient bytes, stretch with simple mixing
    uint64_t acc = UINT64_C(0x9E3779B97F4A7C15);
    for (size_t i = 0; i < n; ++i)
        acc = acc * UINT64_C(0x9E3779B97F4A7C15) + (uint64_t)buf[i];

    // Fill state
    for (int i = 0; i < 4; ++i) {
        acc ^= acc >> 33;
        acc *= UINT64_C(0xff51afd7ed558ccd);
        acc ^= acc >> 33;
        acc *= UINT64_C(0xc4ceb9fe1a85ec53);
        acc ^= acc >> 33;
        s_state[i] = (uint64_t)(acc + ((uint64_t)i) * UINT64_C(0x9E3779B97F4A7C15));
    }
}

void po_rand_seed(uint64_t seed) {
    // reset state for deterministic seeding
    s_state[0] = s_state[1] = s_state[2] = s_state[3] = 0;
    uint8_t b[8];
    for (int i = 0; i < 8; ++i)
        b[i] = (uint8_t)((seed >> (i * 8)) & 0xFF);
    seed_from_bytes(b, sizeof(b));
    // Warm up
    for (int i = 0; i < 8; ++i)
        (void)xoshiro256ss();
}

void po_rand_seed_auto(void) {
    // reset state for deterministic seeding step
    s_state[0] = s_state[1] = s_state[2] = s_state[3] = 0;
    uint8_t buf[32];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, buf, sizeof(buf));
        close(fd);
        if (n == (ssize_t)sizeof(buf)) {
            seed_from_bytes(buf, sizeof(buf));
            for (int i = 0; i < 8; ++i)
                (void)xoshiro256ss();
            return;
        }
    }
    // Fallback: time-based
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t mix = ((uint64_t)ts.tv_sec << 32) ^ (uint64_t)ts.tv_nsec ^ (uint64_t)getpid();
    po_rand_seed(mix);
}

uint64_t po_rand_u64(void) {
    if (!seeded())
        po_rand_seed_auto();
    return xoshiro256ss();
}

uint32_t po_rand_u32(void) { return (uint32_t)(po_rand_u64() >> 32); }

int64_t po_rand_range_i64(int64_t min, int64_t max) {
    if (min > max) {
        int64_t t = min;
        min = max;
        max = t;
    }
    uint64_t span = (uint64_t)((max - min) + 1);
    uint64_t x = po_rand_u64();
    return (int64_t)(min + (int64_t)(x % span));
}

double po_rand_unit(void) {
    // 53-bit precision fraction in [0,1)
    const double norm = 0x1.0p-53; // 1 / 2^53
    return (double)(po_rand_u64() >> 11) * norm;
}

void po_rand_shuffle(void *base, size_t n, size_t elem_size) {
    if (!base || elem_size == 0 || n < 2)
        return;
    unsigned char *a = (unsigned char *)base;
    for (size_t i = n - 1; i > 0; --i) {
        size_t j = (size_t)po_rand_range_i64(0, (int64_t)i);
        if (j == i)
            continue;
        // swap elements a[i] and a[j]
        for (size_t b = 0; b < elem_size; ++b) {
            unsigned char tmp = a[i * elem_size + b];
            a[i * elem_size + b] = a[j * elem_size + b];
            a[j * elem_size + b] = tmp;
        }
    }
}
