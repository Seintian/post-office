/**
 * @file hashtable.h
 * @ingroup hashtable
 * @brief Hash table (key -> value) with chaining collision resolution, dynamic
 *        resizing, and optional iteration helpers.
 *
 * Design Overview
 * ---------------
 *  - Collision Resolution: Separate chaining with singly-linked lists. Each
 *        bucket maintains a chain of nodes for handling hash collisions.
 *  - Resizing: Capacity expands to the next prime when load factor exceeds an
 *    internal high watermark (e.g. ~0.70). It may shrink when the load factor
 *    falls below a low watermark to reclaim memory (hysteresis prevents rapid
 *    oscillation). Exact thresholds are internal implementation details.
 *  - Hash Function: User supplied; high-quality dispersion is essential to
 *    maintain expected O(1) average operation time and reduce primary cluster
 *    formation. Poor hash quality degrades toward O(n).
 *  - Equality: User supplied compare function (0 => equal) defines key
 *    equivalence, enabling opaque pointer keys or custom structs.
 *  - Memory: Table stores raw key + value pointers; does NOT copy or free the
 *    pointed-to data. Callers manage lifetimes.
 *
 * Big-O (Expected / Amortized)
 * ---------------------------
 *  - put / get / contains / remove: O(1) expected, O(n) worst-case.
 *  - resize: O(n) when it occurs, amortized across many operations.
 *
 * Iteration Semantics
 * -------------------
 *  - Iterator traverses current occupied slots (keys present at iterator
 *    creation plus any that remain reachable). Mutating the table (put/remove)
 *    during iteration may invalidate the iterator (unless implementation
 *    explicitly documents safety; assume NOT safe by default).
 *  - Order is unspecified and may change after rehash.
 *
 * Concurrency
 * -----------
 * Not thread-safe. External synchronization required for concurrent access.
 *
 * Error Handling
 * --------------
 *  - Creation returns NULL on allocation failure (errno typically ENOMEM).
 *  - put returns -1 on allocation / resize failure.
 *  - remove returns 0 if key absent.
 *
 * Value Replacement
 * -----------------
 *  - put updates existing key's value in-place (returns 0 in that case).
 *  - replace updates only if key exists (returns 1 on success, 0 otherwise).
 *
 * Debug / Testing Aids
 * --------------------
 *  - load_factor queries current occupancy ratio.
 *  - keyset / values allocate snapshots of keys / values for enumeration or
 *    test assertions.
 *
 * @see prime.h (capacity growth sequence helper)
 * @see hashset.h For the set-only variant sharing probing / resizing logic.
 * @see po_hashtable_put
 * @see po_hashtable_remove
 */

#ifndef PO_HASHTABLE_H
#define PO_HASHTABLE_H

// *** INCLUDES *** //

#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// *** TYPEDEFS *** //

typedef struct po_hashtable po_hashtable_t;
typedef struct po_hashtable_iter po_hashtable_iter_t;

// *** API *** // NOTE: canonical po_hashtable_* names

/**
 * @brief Create a new hash table with default prime capacity (e.g. 17).
 * @param compare Equality predicate (0 => equal).
 * @param hash_func Hash function mapping key -> unsigned long.
 * @return New table handle or NULL on allocation failure (errno set).
 */
po_hashtable_t *po_hashtable_create(int (*compare)(const void *, const void *),
                                    unsigned long (*hash_func)(const void *))
    __nonnull((1, 2)) __attribute_malloc__;

/**
 * @brief Create a table with explicit base capacity.
 * @param compare Equality predicate.
 * @param hash_func Hash function.
 * @param base_capacity Suggested starting capacity (prime recommended). Implementation may round
 *        to next prime.
 * @return New table or NULL on allocation failure.
 */
po_hashtable_t *po_hashtable_create_sized(int (*compare)(const void *, const void *),
                                          unsigned long (*hash_func)(const void *),
                                          size_t base_capacity)
    __nonnull((1, 2)) __attribute_malloc__;

// *** Basic hash table operations *** //

/**
 * @brief Insert or update (key,value).
 *
 * Triggers resize when post-insert load factor would exceed high watermark.
 * @param table Table handle.
 * @param key Key pointer.
 * @param value Value pointer.
 * @return 1 inserted new pair; 0 updated existing; -1 on allocation / internal error.
 */
int po_hashtable_put(po_hashtable_t *table, void *key, void *value) __nonnull((1, 2));

/**
 * @brief Lookup value for key.
 * @param table Table handle.
 * @param key Key pointer.
 * @return Value pointer or NULL if absent.
 */
void *po_hashtable_get(const po_hashtable_t *table, const void *key) __nonnull((1, 2));

/**
 * @brief Membership test.
 * @return 1 present; 0 absent.
 */
int po_hashtable_contains_key(const po_hashtable_t *table, const void *key) __nonnull((1, 2));

/**
 * @brief Remove key (if present); may trigger shrink at low watermark.
 * @return 1 removed; 0 not found.
 */
int po_hashtable_remove(po_hashtable_t *table, const void *key) __nonnull((1, 2));

/** @brief Number of stored key-value pairs. */
size_t po_hashtable_size(const po_hashtable_t *table) __nonnull((1));

/** @brief Current bucket capacity. */
size_t po_hashtable_capacity(const po_hashtable_t *table) __nonnull((1));

/**
 * @brief Snapshot all keys into a newly allocated array (caller frees array).
 * Array length equals ::po_hashtable_size(); not NULL-terminated.
 */
void **po_hashtable_keyset(const po_hashtable_t *table) __nonnull((1)) __attribute_malloc__;

/**
 * @brief Destroy table (keys/values not freed) and NULL handle.
 */
void po_hashtable_destroy(po_hashtable_t **table) __nonnull((1));

// *** Extended hash table operations *** //

/**
 * @brief Allocate an iterator positioned at the first occupied slot.
 *
 * Not safe against concurrent structural modification (put/remove) of @p ht.
 */
po_hashtable_iter_t *po_hashtable_iterator(const po_hashtable_t *ht);

/**
 * @brief Advance iterator to next occupied slot.
 * @return true if a new element is available; false if end reached.
 */
bool po_hashtable_iter_next(po_hashtable_iter_t *it);

/** @brief Current key (undefined if last next() returned false). */
void *po_hashtable_iter_key(const po_hashtable_iter_t *it);

/** @brief Current value (undefined if last next() returned false). */
void *po_hashtable_iter_value(const po_hashtable_iter_t *it);

/**
 * @brief Current load factor (size / capacity).
 */
float po_hashtable_load_factor(const po_hashtable_t *table) __nonnull((1));

/**
 * @brief Replace existing key's value without inserting if absent.
 * @return 1 replaced; 0 key not found.
 */
int po_hashtable_replace(const po_hashtable_t *table, const void *key, void *new_value)
    __nonnull((1, 2));

/**
 * @brief Remove all key-value pairs (capacity may remain; keys/values untouched).
 * @return 1 cleared; 0 already empty.
 */
int po_hashtable_clear(po_hashtable_t *table) __nonnull((1));

/**
 * @brief Apply @p func to each (key,value) in unspecified order.
 * Function must not mutate the table structurally (put/remove) or free keys/values.
 */
void po_hashtable_map(const po_hashtable_t *table, void (*func)(void *key, void *value))
    __nonnull((1, 2));

/**
 * @brief Snapshot all values into a newly allocated array (caller frees).
 * Array length equals ::po_hashtable_size(); not NULL-terminated.
 */
void **po_hashtable_values(const po_hashtable_t *table) __nonnull((1)) __attribute_malloc__;

/**
 * @brief Compare two tables for key set + value equality (by provided value compare).
 * @return 1 equal; 0 not equal; -1 on internal error.
 */
int po_hashtable_equals(const po_hashtable_t *table1, const po_hashtable_t *table2,
                        int (*compare)(const void *, const void *)) __nonnull((1, 2, 3));

/**
 * @brief Shallow copy (keys/values pointer-copied) of @p table.
 * @return New table or NULL on failure.
 */
po_hashtable_t *po_hashtable_copy(const po_hashtable_t *table) __nonnull((1)) __attribute_malloc__;

/**
 * @brief Merge source into dest (replacing values for existing keys).
 */
void po_hashtable_merge(po_hashtable_t *dest, const po_hashtable_t *source) __nonnull((1, 2));

#ifdef __cplusplus
}
#endif

#endif // PO_HASHTABLE_H
