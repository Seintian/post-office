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

#ifndef _PRIME_H
#define _PRIME_H

#include <stdbool.h>
#include <sys/cdefs.h>
#include <stddef.h>

/**
 * @brief checks if a number is prime
 *
 * Checks if a number is prime.
 *
 * @param[in] n The number to check
 * @return `true` if the number is prime, `false` otherwise
 */
bool is_prime(size_t n) __wur;

/**
 * @brief gets the next prime number after a given number
 *
 * Gets the next prime number after a given number,
 * regardless of whether the number is prime.
 *
 * @param[in] n The number to start from
 * @return the next prime number after n
 */
size_t next_prime(size_t n) __wur;

#endif // _PRIME_H_