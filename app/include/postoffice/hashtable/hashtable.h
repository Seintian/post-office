/**
 * @file hashtable.h
 * @brief Declares the HashTable API.
 * 
 * @see hashtable.c
 * @see prime.h
 */

#ifndef _HASHTABLE_H
#define _HASHTABLE_H

// *** INCLUDES *** //

#include <stdlib.h>
#include <sys/types.h>

// *** TYPEDEFS *** //

typedef struct _hash_node_t hash_node_t;
typedef struct _hash_table_t hash_table_t;

// *** API *** //

/**
 * @brief creates a new HashTable
 * 
 * Initializes a new hash table with the given comparison and hash functions and a default initial capacity.
 * 
 * @param[in] compare The comparison function for keys:
 *                    should return 0 if keys are equal, non-zero otherwise
 * @param[in] hash_func The hash function for keys:
 *                      should return a unique hash (unsigned long) for each key
 * @return Pointer to the newly created hash table on success, or `NULL` on failure
 * 
 * @note The default initial capacity is 17.
 * @note The caller is responsible for freeing the hash table.
 */
hash_table_t* hash_table_create(
    int (*compare)(const void*, const void*),
    unsigned long (*hash_func)(const void*)
) __wur __nonnull((1, 2)) __attribute_malloc__;

/**
 * @brief creates a new HashTable with a specified base capacity
 * 
 * Initializes a new hash table with the given comparison,
 * hash functions and a specified base capacity.
 * 
 * @param[in] compare The comparison function for keys:
 *                    should return 0 if keys are equal, non-zero otherwise
 * @param[in] hash_func The hash function for keys:
 *                      should return a unique hash (unsigned long) for each key
 * @param[in] base_capacity The base capacity for the new hash table
 * @return Pointer to the newly created hash table on success, or `NULL` on failure
 * 
 * @note The caller is responsible for freeing the hash table.
 * @note The base capacity should be a prime number.
 */
hash_table_t* hash_table_create_sized(
    int (*compare)(const void*, const void*),
    unsigned long (*hash_func)(const void*),
    size_t base_capacity
) __wur __nonnull((1, 2)) __attribute_malloc__;

// *** Basic hash table operations *** //

/**
 * @brief puts a key-value pair into the hash table
 * 
 * Inserts a key-value pair into the hash table. If the key already exists, the value is updated.
 * If the load factor exceeds the threshold, the table is resized.
 * 
 * ```
 * table -> capacity = next_prime(table -> capacity * 2)
 * ```
 * 
 * @param[in] table The hash table to insert into
 * @param[in] key The key to insert
 * @param[in] value The value to insert
 * @return 1 if a new key-value pair was inserted, 0 if the key already existed and the value was updated, -1 on failure
 */
int hash_table_put(
    hash_table_t* table,
    void* key,
    void* value
) __nonnull((1, 2));

/**
 * @brief gets the value for a given key
 * 
 * Retrieves the value for a given key from the hash table. If the key does not exist, returns NULL.
 * 
 * @param[in] table The hash table to retrieve from
 * @param[in] key The key to retrieve
 * @return void* corresponding to the value for the given key, NULL if the key does not exist or the table is NULL
 */
void* hash_table_get(
    const hash_table_t* table,
    const void* key
) __wur __nonnull((1, 2));

/**
 * @brief checks if a key exists in the hash table
 * 
 * Checks if a key exists in the hash table.
 * Returns 1 if the key exists, 0 otherwise.
 * 
 * @param[in] table The hash table to check
 * @param[in] key The key to check for
 * @return integer 1 if the key exists, 0 if it does not
 */
int hash_table_contains_key(
    const hash_table_t* table,
    const void* key
) __wur __nonnull((1, 2));

/**
 * @brief removes a key-value pair from the hash table
 * 
 * Removes a key-value pair from the hash table.
 * If the load factor falls below the threshold, the table is resized.
 * 
 * ```
 * table -> capacity = next_prime(table -> capacity / 2)
 * ```
 * 
 * @param[in] table The hash table to remove from
 * @param[in] key The key to remove
 * @return 1 on success, 0 if the key does not exist
 */
int hash_table_remove(hash_table_t* table, const void* key) __nonnull((1, 2));

/**
 * @brief gets the current size of the hash table
 * 
 * Returns the current number of key-value pairs in the hash table.
 * 
 * @param[in] table The hash table to get the size of
 * @return integer representing the size of the hash table, or -1 on failure
 */
size_t hash_table_size(const hash_table_t* table) __wur __nonnull((1));

/**
 * @brief gets the current capacity of the hash table
 * 
 * Returns the current capacity of the hash table (number of buckets).
 * 
 * @param[in] table The hash table to get the capacity of
 * @return integer representing the capacity of the hash table, or -1 on failure
 */
size_t hash_table_capacity(const hash_table_t* table) __wur __nonnull((1));

/**
 * @brief gets an array of all keys in the hash table
 * 
 * Returns an array of all keys in the hash table.
 * 
 * @param[in] table The hash table to get the keys from
 * @return void** corresponding to an array of keys in the hash table, or `NULL` on failure
 * 
 * @note The caller is responsible for freeing the array of keys.
 */
void** hash_table_keyset(
    const hash_table_t* table
) __wur __nonnull((1)) __attribute_malloc__;

/**
 * @brief frees the hash table
 * 
 * Frees the hash table and all associated memory. Does not free the keys or values themselves.
 * 
 * @param[in] table The hash table to free
 */
void hash_table_free(hash_table_t* table) __nonnull((1));

// *** Extended hash table operations *** //

/**
 * @brief gets the current load factor of the hash table
 * 
 * Returns the current load factor of the hash table (size / capacity).
 * 
 * @param[in] table The hash table to get the load factor of
 * @return float representing the load factor of the hash table, or -1 on failure
 */
float hash_table_load_factor(const hash_table_t* table) __wur __nonnull((1));

/**
 * @brief replaces the value for a given key
 * 
 * Replaces the value for a given key in the hash table.
 * 
 * @param[in] table The hash table to replace in
 * @param[in] key The key to replace
 * @param[in] new_value The new value to insert
 * @return 1 on success, 0 if the key does not exist
 */
int hash_table_replace(
    const hash_table_t* table,
    const void* key,
    void* new_value
) __nonnull((1, 2));

/**
 * @brief clears all elements from the hash table
 * 
 * Clears all elements from the hash table, freeing all associated memory.
 * Does not free the keys or values themselves.
 * Does not free the bucket array itself, either.
 * 
 * @param[in] table The hash table to clear
 * @return 1 on success, 0 if the table is already empty
 */
int hash_table_clear(hash_table_t* table) __nonnull((1));

/**
 * @brief maps a function over all key-value pairs in the hash table
 * 
 * The function should take two generic pointers (key, value).
 * 
 * @param[in] table The hash table to map over
 * @param[in] func The function to map: should take two generic pointers (key, value)
 * 
 * @note The order of the key-value pairs is not guaranteed.
 * @note The function should not free the key or value pointers.
 */
void hash_table_map(
    const hash_table_t* table,
    void (*func)(void* key, void* value)
) __nonnull((1, 2));

/**
 * @brief gets an array of all values in the hash table
 * 
 * Returns an array of all values in the hash table.
 * The array is allocated on the heap and must be freed by the caller.
 * 
 * @param[in] table The hash table to get the values from
 * @return The array of values in the hash table, or `NULL` on failure
 * 
 * @note The caller is responsible for freeing the array of values.
 */
void** hash_table_values(
    const hash_table_t* table
) __wur __nonnull((1)) __attribute_malloc__;

/**
 * @brief checks if two hash tables are equal
 * 
 * Checks if two hash tables are equal.
 * Returns 1 if the hash tables are equal, 0 otherwise.
 * 
 * @param[in] table1 The first hash table to compare
 * @param[in] table2 The second hash table to compare
 * @param[in] compare The comparison function for values:
 *                    should return 0 if values are equal, non-zero otherwise
 * @return 1 if the hash tables are equal, 0 if they are not, or -1 on failure
 */
int hash_table_equals(
    const hash_table_t* table1,
    const hash_table_t* table2,
    int (*compare)(const void*, const void*)
) __wur __nonnull((1, 2, 3));

/**
 * @brief copies a hash table
 * 
 * Copies a hash table, creating a new hash table with the same keys and values.
 * All keys and values are copied by reference, not by value.
 * 
 * @param[in] table The original hash table to copy
 * @return Pointer to the new hash table - copy, or `NULL` on failure
 * 
 * @note The caller is responsible for freeing the new hash table.
 */
hash_table_t* hash_table_copy(
    const hash_table_t* table
) __wur __nonnull((1)) __attribute_malloc__;

/**
 * @brief merges two hash tables
 * 
 * Merges two hash tables, copying all key-value pairs from the source table to the destination table.
 * If a key already exists in the destination table, the value is replaced.
 * 
 * @param[in] dest The destination hash table
 * @param[in] source The source hash table
 */
void hash_table_merge(
    hash_table_t* dest,
    const hash_table_t* source
) __nonnull((1, 2));

#endif // _HASHTABLE_H
