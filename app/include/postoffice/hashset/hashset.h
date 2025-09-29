#ifndef PO_HASHSET_H
#define PO_HASHSET_H

/**
 * @file hashset.h
 * @ingroup hashset
 * @brief Open-addressed (linear probing) hash set for storing unique keys.
 *
 * Design Overview
 * ---------------
 *  - Collision Resolution: Linear probing (open addressing). Probe sequence:
 *        `h, (h+1) % capacity, (h+2) % capacity, ...` until empty / tombstone / match.
 *  - Resizing: Capacity grows to the next prime when load factor exceeds an
 *    internal upper threshold (e.g. ~0.70). Optional downsize may occur when
 *    deletions lower load factor below a lower threshold (implementation
 *    dependent; not guaranteed if hysteresis is used to avoid thrash).
 *  - Hash Quality: User-provided @p hash_func should distribute well; poor
 *    distribution increases clustering and degrades performance.
 *  - Key Uniqueness: User-provided @p compare defines equality (0 => equal).
 *
 * Big-O Characteristics (expected, under a good hash):
 *  - Insert / Contains / Remove: Amortized O(1); worst-case O(n) in a fully
 *    clustered table.
 *  - Resize: O(n) (rehashes all occupied slots) but amortized over many ops.
 *
 * Memory / Ownership
 * ------------------
 *  - The set stores raw key pointers; it does NOT copy nor free user keys.
 *  - Callers must ensure the lifetime of inserted keys spans their presence in
 *    the set. Removing or destroying the set leaves keys untouched.
 *
 * Error Handling
 * --------------
 *  - Creation returns NULL on allocation failure (errno=ENOMEM typically).
 *  - Insert returns -1 on allocation failure during resize or internal error.
 *  - All functions require non-NULL handle (unless otherwise stated) and may
 *    invoke undefined behavior if used incorrectly (defensive checks minimal
 *    for performance).
 *
 * Iteration
 * ---------
 * This API does not currently expose an iterator; callers wanting a snapshot
 * may use ::po_hashset_keys() which allocates an array of current keys.
 *
 * @note Default initial capacity is a prime (17) to reduce clustering early.
 * @note Load factor definition: size / capacity.
 *
 * @see hashtable.h For associative key->value variant built on similar probing
 *                  semantics.
 * @see po_hashset_add
 * @see po_hashset_remove
 */

#include <stdlib.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque hashset handle (contents private to implementation)
typedef struct po_hashset po_hashset_t;

/**
 * @brief Create a new hash set with default prime capacity.
 *
 * @param compare   Equality predicate (returns 0 for equality).
 * @param hash_func Hash function mapping key -> unsigned long hash.
 * @return New set handle or NULL on allocation failure (errno set).
 */
po_hashset_t *po_hashset_create(int (*compare)(const void *, const void *),
                                unsigned long (*hash_func)(const void *)) __attribute_malloc__
    __nonnull((1, 2));

/**
 * @brief Create a hash set with explicit initial capacity.
 *
 * @param compare          Equality predicate.
 * @param hash_func        Hash function.
 * @param initial_capacity Suggested starting capacity (prime recommended). If not prime the
 *                          implementation may round up to the next prime.
 * @return New set handle or NULL on allocation failure.
 */
po_hashset_t *po_hashset_create_sized(int (*compare)(const void *, const void *),
                                      unsigned long (*hash_func)(const void *),
                                      size_t initial_capacity) __attribute_malloc__
    __nonnull((1, 2));

/**
 * @brief Insert a key (no-op if already present).
 *
 * May trigger resize if post-insert load factor would exceed the configured
 * growth threshold. Key pointer is stored verbatim – caller retains allocation
 * responsibility.
 *
 * @param set Set handle.
 * @param key Key pointer.
 * @return 1 inserted; 0 already existed; -1 on allocation / internal error.
 */
int po_hashset_add(po_hashset_t *set, void *key) __nonnull((1, 2));

/**
 * @brief Remove a key (if present).
 *
 * Uses a tombstone strategy to preserve probe chains. Optional shrink may
 * occur after removal if load factor falls below a lower threshold.
 *
 * @param set Set handle.
 * @param key Key to remove.
 * @return 1 removed; 0 not found.
 */
int po_hashset_remove(po_hashset_t *set, const void *key) __nonnull((1, 2));

/**
 * @brief Test membership.
 * @param set Set handle.
 * @param key Key to search for.
 * @return 1 present; 0 absent.
 */
int po_hashset_contains(const po_hashset_t *set, const void *key) __nonnull((1, 2));

/**
 * @brief Current number of stored keys.
 * @param set Set handle.
 * @return Count (>=0).
 */
size_t po_hashset_size(const po_hashset_t *set) __nonnull((1));

/**
 * @brief Current bucket capacity.
 * @param set Set handle.
 * @return Capacity (number of slots).
 */
size_t po_hashset_capacity(const po_hashset_t *set) __nonnull((1));

/**
 * @brief Snapshot the current key pointers into a newly allocated array.
 *
 * Caller frees the returned array (keys are not copied nor owned by the set).
 * @note The array length equals ::po_hashset_size(); it is NOT NULL-terminated.
 *
 * @param set Set handle.
 * @return Pointer to array or NULL on allocation failure.
 */
void **po_hashset_keys(const po_hashset_t *set) __attribute_malloc__ __nonnull((1));

/**
 * @brief Remove all keys (capacity unchanged, keys not freed).
 * @param set Set handle.
 */
void po_hashset_clear(po_hashset_t *set) __nonnull((1));

/**
 * @brief Destroy the set and free internal storage (not keys).
 * @param set Address of set handle; pointer is nulled on return.
 */
void po_hashset_destroy(po_hashset_t **set) __nonnull((1));

/**
 * @brief Current load factor (size / capacity).
 * @param set Set handle.
 * @return Load factor as float.
 */
float po_hashset_load_factor(const po_hashset_t *set) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // PO_HASHSET_H
