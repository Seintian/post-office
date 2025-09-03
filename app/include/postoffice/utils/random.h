/**
 * @file random.h
 * @brief Small random utilities (seed, integers, doubles, shuffling).
 * @ingroup utils
 */

#ifndef POSTOFFICE_UTILS_RANDOM_H
#define POSTOFFICE_UTILS_RANDOM_H

#include <stdint.h>
#include <stddef.h>

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
