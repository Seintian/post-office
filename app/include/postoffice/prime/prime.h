/**
 * @file prime.h
 * @ingroup prime
 * @brief Prime number helpers for sizing hash-based data structures.
 *
 * Provides simple predicates to test primality and compute the next prime >= n.
 * Used by hash table / set implementations to select capacities that reduce
 * clustering under linear probing.
 *
 * Complexity
 * ----------
 *  - is_prime: O(sqrt(n)) trial division.
 *  - next_prime: Iteratively applies is_prime until a prime is found.
 *
 * For large dynamic tables this cost is amortized across infrequent resize
 * events.
 *
 * @see hashtable.h
 * @see hashset.h
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
 * @brief Checks if a number is prime.
 *
 * Checks if a number is prime.
 *
 * @param[in] n The number to check
 * @return `true` if the number is prime, `false` otherwise
 * @note Thread-safe: Yes.
 */
bool is_prime(size_t n);

/**
 * @brief Gets the next prime number after a given number.
 *
 * Gets the next prime number after a given number,
 * regardless of whether the number is prime.
 *
 * @param[in] n The number to start from
 * @return the next prime number after n
 * @note Thread-safe: Yes.
 */
size_t next_prime(size_t n);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _PRIME_H_