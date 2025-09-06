#ifndef PO_HASHSET_H
#define PO_HASHSET_H

/**
 * @file hashset.h
 * @brief Declares the HashSet API for storing unique keys.
 * @ingroup libraries
 *
 * A HashSet stores only keys and ensures uniqueness. It uses open addressing
 * with linear probing (in implementation) and resizes when load factor thresholds
 * are crossed. Keys are compared via a user-supplied compare function and hashed
 * by a user-supplied hash function.
 *
 * @note The caller is responsible for managing key memory (insertion expects
 *       ownership transfer or persistent storage of keys).
 * @note The default initial capacity is 17 (a prime number).
 */

#include <stdlib.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque hashset handle (contents private to implementation)
typedef struct po_hashset po_hashset_t;

/**
 * @brief Create a new HashSet with default capacity.
 *
 * @param[in] compare   Function to compare keys: returns 0 if equal.
 * @param[in] hash_func Function to hash keys: returns unsigned long hash.
 * @return Pointer to the new set, or NULL on failure.
 */
po_hashset_t *po_hashset_create(int (*compare)(const void *, const void *),
                                unsigned long (*hash_func)(const void *)) __attribute_malloc__
    __nonnull((1, 2));

/**
 * @brief Create a new HashSet with specified initial capacity.
 *
 * @param[in] compare        Function to compare keys.
 * @param[in] hash_func      Function to hash keys.
 * @param[in] initial_capacity Prime number initial capacity.
 * @return Pointer to the new set, or NULL on failure.
 */
po_hashset_t *po_hashset_create_sized(int (*compare)(const void *, const void *),
                                      unsigned long (*hash_func)(const void *),
                                      size_t initial_capacity) __attribute_malloc__
    __nonnull((1, 2));

/**
 * @brief Insert a key into the set.
 *
 * If the key already exists, no action is taken.
 * Resizes the set if load factor exceeds threshold.
 *
 * @param[in] set  The set to add to.
 * @param[in] key  The key to insert.
 * @return 1 if inserted, 0 if already present, -1 on error.
 */
int po_hashset_add(po_hashset_t *set, void *key) __nonnull((1, 2));

/**
 * @brief Remove a key from the set.
 *
 * @param[in] set  The set to remove from.
 * @param[in] key  The key to remove.
 * @return 1 if removed, 0 if not found.
 */
int po_hashset_remove(po_hashset_t *set, const void *key) __nonnull((1, 2));

/**
 * @brief Check if a key exists in the set.
 *
 * @param[in] set  The set to check.
 * @param[in] key  The key to find.
 * @return 1 if present, 0 otherwise.
 */
int po_hashset_contains(const po_hashset_t *set, const void *key) __nonnull((1, 2));

/**
 * @brief Get the number of keys in the set.
 *
 * @param[in] set The set to query.
 * @return Number of keys, or 0 if set is NULL.
 */
size_t po_hashset_size(const po_hashset_t *set) __nonnull((1));

/**
 * @brief Get the current capacity (bucket count) of the set.
 *
 * @param[in] set The set to query.
 * @return Current capacity, or 0 on error.
 */
size_t po_hashset_capacity(const po_hashset_t *set) __nonnull((1));

/**
 * @brief Get all keys in the set.
 *
 * Returns an array of key pointers. The caller must free the array
 * but not the keys themselves.
 *
 * @param[in] set The set to extract keys from.
 * @return Array of keys, or NULL on error.
 *
 * @note The array is NOT NULL-terminated. Use size() to determine length.
 */
void **po_hashset_keys(const po_hashset_t *set) __attribute_malloc__ __nonnull((1));

/**
 * @brief Clear all keys from the set.
 *
 * After clearing, size() == 0 but capacity remains unchanged.
 * Does NOT free keys.
 *
 * @param[in] set The set to clear.
 */
void po_hashset_clear(po_hashset_t *set) __nonnull((1));

/**
 * @brief Free the HashSet.
 *
 * Frees internal structures. Does NOT free keys.
 *
 * @param[in] set The set to free.
 */
void po_hashset_destroy(po_hashset_t **set) __nonnull((1));

/**
 * @brief Get the current load factor of the set.
 *
 * Load factor = size / capacity. Returns 0 if set is NULL.
 *
 * @param[in] set The set to query.
 * @return Current load factor, or 0 on error.
 */
float po_hashset_load_factor(const po_hashset_t *set) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // PO_HASHSET_H
