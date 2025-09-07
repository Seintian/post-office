/**
 * @file prime.h
 * @brief Functions for prime number operations.
 * @ingroup libraries
 *
 * This file contains the declarations for functions that check if a number is prime,
 * and find the next prime number after a given number.
 * Primarily used for hash tables.
 *
 * @see hashtable.h
 */

// NOTE: Added C++ linkage guards for compatibility.
#ifndef _PRIME_H
#define _PRIME_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief checks if a number is prime
 *
 * Checks if a number is prime.
 *
 * @param[in] n The number to check
 * @return `true` if the number is prime, `false` otherwise
 */
bool is_prime(size_t n);

/**
 * @brief gets the next prime number after a given number
 *
 * Gets the next prime number after a given number,
 * regardless of whether the number is prime.
 *
 * @param[in] n The number to start from
 * @return the next prime number after n
 */
size_t next_prime(size_t n);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _PRIME_H_