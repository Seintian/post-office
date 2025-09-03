/**
 * @file prime.c
 * @brief Implementation of functions for prime number operations.
 *
 * @see prime.h
 */

#include "prime/prime.h"

#include <stdlib.h>

bool is_prime(size_t n) {
    if (n <= 1)
        return false;

    if (n <= 3)
        return true;

    if (n % 2 == 0 || n % 3 == 0)
        return false;

    for (size_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }

    return true;
}

size_t next_prime(size_t n) {
    if (n <= 1)
        return 2;

    size_t prime = n;
    size_t found = 0;

    while (!found) {
        prime++;

        if (is_prime(prime))
            found = 1;
    }

    return prime;
}
