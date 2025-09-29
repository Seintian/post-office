/**
 * @file random.h
 * @ingroup utils
 * @brief Small random utilities (seed, integers, doubles, shuffling).
 *
 * Features
 * --------
 *  - Deterministic seeding via ::po_rand_seed for reproducible tests.
 *  - Automatic high-entropy seeding fallback ::po_rand_seed_auto.
 *  - Convenience uniform generation for 32/64-bit integers, double in [0,1),
 *    bounded integer ranges (inclusive), and Fisher-Yates array shuffle.
 *
 * Thread-Safety
 * -------------
 * Implementation uses a single internal PRNG state (not thread-safe). Callers
 * requiring thread-local streams should layer their own state objects or guard
 * usage with a mutex.
 */

#ifndef POSTOFFICE_UTILS_RANDOM_H
#define POSTOFFICE_UTILS_RANDOM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Initialize RNG with high-entropy seed (from /dev/urandom),
 *        falling back to time-based seed if unavailable.
 */
void po_rand_seed_auto(void);

/**
 * @brief Initialize RNG with a given 64-bit seed.
 */
void po_rand_seed(uint64_t seed);

/**
 * @brief Return uniformly distributed 64-bit value.
 */
uint64_t po_rand_u64(void);

/**
 * @brief Return uniformly distributed 32-bit value.
 */
uint32_t po_rand_u32(void);

/**
 * @brief Return integer in [min, max] inclusive. If min>max, arguments are swapped.
 */
int64_t po_rand_range_i64(int64_t min, int64_t max);

/**
 * @brief Return double in [0,1).
 */
double po_rand_unit(void);

/**
 * @brief Fisher-Yates shuffle for an array of elements of size elem_size.
 */
void po_rand_shuffle(void *base, size_t n, size_t elem_size);

#ifdef __cplusplus
}
#endif

#endif // POSTOFFICE_UTILS_RANDOM_H
