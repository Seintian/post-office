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
 * @param[in] compare Equality predicate (0 => equal).
 * @param[in] hash_func Hash function mapping key -> unsigned long.
 * @return New table handle or NULL on allocation failure (errno set).
 * @note Thread-safe: Yes.
 */
po_hashtable_t *po_hashtable_create(int (*compare)(const void *, const void *),
                                    unsigned long (*hash_func)(const void *))
    __nonnull((1, 2)) __attribute_malloc__;

/**
 * @brief Create a table with explicit base capacity.
 * @param[in] compare Equality predicate.
 * @param[in] hash_func Hash function.
 * @param[in] base_capacity Suggested starting capacity (prime recommended).
 * @return New table or NULL on allocation failure.
 * @note Thread-safe: Yes.
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
 *
 * @param[in] table Table handle.
 * @param[in] key Key pointer.
 * @param[in] value Value pointer.
 * @return 1 inserted new pair; 0 updated existing; -1 on allocation / internal error.
 * @note Thread-safe: No.
 */
int po_hashtable_put(po_hashtable_t *restrict table, void *key, void *value) __nonnull((1, 2));

/**
 * @brief Lookup value for key.
 * @param[in] table Table handle.
 * @param[in] key Key pointer.
 * @return Value pointer or NULL if absent.
 * @note Thread-safe: Yes (Read-only on table).
 */
void *po_hashtable_get(const po_hashtable_t *restrict table, const void *key) __nonnull((1, 2));

/**
 * @brief Membership test.
 * @param[in] table Table handle.
 * @param[in] key Key pointer.
 * @return 1 present; 0 absent.
 * @note Thread-safe: Yes (Read-only on table).
 */
int po_hashtable_contains_key(const po_hashtable_t *restrict table, const void *key) __nonnull((1, 2));

/**
 * @brief Remove key (if present); may trigger shrink at low watermark.
 * @param[in] table Table handle.
 * @param[in] key Key pointer.
 * @return 1 removed; 0 not found.
 * @note Thread-safe: No.
 */
int po_hashtable_remove(po_hashtable_t *restrict table, const void *key) __nonnull((1, 2));

/** 
 * @brief Number of stored key-value pairs. 
 * @param[in] table Table handle.
 * @note Thread-safe: Yes (Read-only).
 */
size_t po_hashtable_size(const po_hashtable_t *table) __nonnull((1));

/** 
 * @brief Current bucket capacity. 
 * @param[in] table Table handle.
 * @note Thread-safe: Yes (Read-only).
 */
size_t po_hashtable_capacity(const po_hashtable_t *table) __nonnull((1));

/**
 * @brief Snapshot all keys into a newly allocated array (caller frees array).
 * Array length equals ::po_hashtable_size(); not NULL-terminated.
 * @param[in] table Table handle.
 * @note Thread-safe: Yes (Read-only on table).
 */
void **po_hashtable_keyset(const po_hashtable_t *table) __nonnull((1)) __attribute_malloc__;

/**
 * @brief Destroy table (keys/values not freed) and NULL handle.
 * @param[in,out] table Address of table pointer.
 * @note Thread-safe: No (Must be exclusive).
 */
void po_hashtable_destroy(po_hashtable_t **restrict table) __nonnull((1));

// *** Extended hash table operations *** //

/**
 * @brief Allocate an iterator positioned at the first occupied slot.
 *
 * Not safe against concurrent structural modification (put/remove) of @p ht.
 * @param[in] ht Table handle.
 * @note Thread-safe: Yes (Read-only on table).
 */
po_hashtable_iter_t *po_hashtable_iterator(const po_hashtable_t *ht);

/**
 * @brief Advance iterator to next occupied slot.
 * @param[in] it Iterator.
 * @return true if a new element is available; false if end reached.
 * @note Thread-safe: No.
 */
bool po_hashtable_iter_next(po_hashtable_iter_t *it);

/** 
 * @brief Current key (undefined if last next() returned false).
 * @param[in] it Iterator. 
 * @note Thread-safe: Yes.
 */
void *po_hashtable_iter_key(const po_hashtable_iter_t *it);

/** 
 * @brief Current value (undefined if last next() returned false).
 * @param[in] it Iterator.
 * @note Thread-safe: Yes.
 */
void *po_hashtable_iter_value(const po_hashtable_iter_t *it);

/**
 * @brief Current load factor (size / capacity).
 * @param[in] table Table handle.
 * @note Thread-safe: Yes (Read-only).
 */
float po_hashtable_load_factor(const po_hashtable_t *table) __nonnull((1));

/**
 * @brief Replace existing key's value without inserting if absent.
 * @param[in] table Table handle.
 * @param[in] key Key to look up.
 * @param[in] new_value Value to set if key exists.
 * @return 1 replaced; 0 key not found.
 * @note Thread-safe: No.
 */
int po_hashtable_replace(const po_hashtable_t *restrict table, const void *key, void *new_value)
    __nonnull((1, 2));

/**
 * @brief Remove all key-value pairs (capacity may remain; keys/values untouched).
 * @param[in] table Table handle.
 * @return 1 cleared; 0 already empty.
 * @note Thread-safe: No.
 */
int po_hashtable_clear(po_hashtable_t *table) __nonnull((1));

/**
 * @brief Apply @p func to each (key,value) in unspecified order.
 * Function must not mutate the table structurally (put/remove) or free keys/values.
 * @param[in] table Table handle.
 * @param[in] func Callback function.
 * @note Thread-safe: Yes (Read-only on table, unless func mutates).
 */
void po_hashtable_map(const po_hashtable_t *table, void (*func)(void *key, void *value))
    __nonnull((1, 2));

/**
 * @brief Snapshot all values into a newly allocated array (caller frees).
 * Array length equals ::po_hashtable_size(); not NULL-terminated.
 * @param[in] table Table handle.
 * @note Thread-safe: Yes (Read-only on table).
 */
void **po_hashtable_values(const po_hashtable_t *table) __nonnull((1)) __attribute_malloc__;

/**
 * @brief Compare two tables for key set + value equality (by provided value compare).
 *
 * Warning: Checks for strict structural equality (buckets/chains). May return false (0)
 * even if tables contain same elements but with different history.
 *
 * @param[in] table1 First table.
 * @param[in] table2 Second table.
 * @param[in] compare Value comparator.
 * @return 1 equal; 0 not equal; -1 on internal error.
 * @note Thread-safe: Yes (Read-only).
 */
int po_hashtable_equals(const po_hashtable_t *table1, const po_hashtable_t *table2,
                        int (*compare)(const void *, const void *)) __nonnull((1, 2, 3));

/**
 * @brief Shallow copy (keys/values pointer-copied) of @p table.
 * @param[in] table Source table.
 * @return New table or NULL on failure.
 * @note Thread-safe: Yes (Read-only).
 */
po_hashtable_t *po_hashtable_copy(const po_hashtable_t *table) __nonnull((1)) __attribute_malloc__;

/**
 * @brief Merge source into dest (replacing values for existing keys).
 * @param[in,out] dest Destination table.
 * @param[in] source Source table.
 * @note Thread-safe: No (Mutates dest).
 */
void po_hashtable_merge(po_hashtable_t *dest, const po_hashtable_t *source) __nonnull((1, 2));

#ifdef __cplusplus
}
#endif

#endif // PO_HASHTABLE_H
