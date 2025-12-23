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
 * Implementation uses Thread-Local Storage (TLS) for the PRNG state.
 * All functions are thread-safe and maintain independent streams per thread.
 */

#ifndef POSTOFFICE_UTILS_RANDOM_H
#define POSTOFFICE_UTILS_RANDOM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Initialize RNG with high-entropy seed (from /dev/urandom).
 *
 * Falls back to time-based seed if /dev/urandom is unavailable.
 *
 * @note Thread-safe: Yes (Thread-local state).
 */
void po_rand_seed_auto(void);

/**
 * @brief Initialize RNG with a given 64-bit seed.
 *
 * @param[in] seed The 64-bit seed.
 *
 * @note Thread-safe: Yes (Thread-local state).
 */
void po_rand_seed(uint64_t seed);

/**
 * @brief Return uniformly distributed 64-bit value.
 *
 * Auto-seeds if not already seeded.
 *
 * @return Random uint64_t.
 *
 * @note Thread-safe: Yes (Thread-local state).
 */
uint64_t po_rand_u64(void);

/**
 * @brief Return uniformly distributed 32-bit value.
 *
 * @return Random uint32_t.
 *
 * @note Thread-safe: Yes (Thread-local state).
 */
uint32_t po_rand_u32(void);

/**
 * @brief Return integer in [min, max] inclusive.
 *
 * If min > max, arguments are swapped.
 *
 * @param[in] min Minimum value.
 * @param[in] max Maximum value.
 * @return Random value in range.
 *
 * @note Thread-safe: Yes.
 */
int64_t po_rand_range_i64(int64_t min, int64_t max);

/**
 * @brief Return double in [0,1).
 *
 * @return Random double in [0.0, 1.0).
 *
 * @note Thread-safe: Yes.
 */
double po_rand_unit(void);

/**
 * @brief Fisher-Yates shuffle for an array of elements.
 *
 * @param[in,out] base Pointer to the start of the array (must not be NULL).
 * @param[in] n Number of elements.
 * @param[in] elem_size Size of each element.
 *
 * @note Thread-safe: Yes.
 */
void po_rand_shuffle(void *base, size_t n, size_t elem_size);

#ifdef __cplusplus
}
#endif

#endif // POSTOFFICE_UTILS_RANDOM_H
