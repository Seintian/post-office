/**
 * @file test_hashtable.h
 * @brief Function prototypes for hash table tests.
 * 
 * This file contains the function prototypes for unit tests related to
 * hash table operations. The tests cover a variety of hash table functions,
 * including creation, insertion, resizing, key updates, key containment,
 * removal, and other operations.
 * 
 * The tests are designed to ensure the correctness and efficiency of the 
 * hash table implementation, and they include checks for edge cases and 
 * common operations.
 * 
 * @see hashtable.h
 */

#ifndef _TEST_HASHTABLE_H
#define _TEST_HASHTABLE_H

/**
 * @brief Tests the creation of a hash table.
 * 
 * This test ensures that the hash table is created successfully, 
 * and its initial state is as expected.
 */
void test_hash_table_create(void);

/**
 * @brief Tests putting and getting key-value pairs in the hash table.
 * 
 * This test checks if key-value pairs can be inserted into the hash table 
 * and then retrieved correctly.
 */
void test_hash_table_put_and_get(void);

/**
 * @brief Tests resizing the hash table.
 * 
 * This test ensures that the hash table resizes correctly when it reaches 
 * its load factor threshold, maintaining the integrity of the stored data.
 */
void test_hash_table_resized(void);

/**
 * @brief Tests updating an existing key in the hash table.
 * 
 * This test verifies that an existing key can be updated with a new value 
 * without affecting other keys in the hash table.
 */
void test_hash_table_update_existing_key(void);

/**
 * @brief Tests if a key exists in the hash table.
 * 
 * This test checks if the hash table correctly identifies whether a key 
 * is present in the table.
 */
void test_hash_table_contains_key(void);

/**
 * @brief Tests removing a key from the hash table.
 * 
 * This test ensures that a key can be removed from the hash table, 
 * and the hash table's state is correctly updated after the removal.
 */
void test_hash_table_remove(void);

/**
 * @brief Tests retrieving the set of keys in the hash table.
 * 
 * This test verifies that the hash table correctly returns a set of all 
 * the keys stored in it.
 */
void test_hash_table_keyset(void);

/**
 * @brief Tests retrieving the values in the hash table.
 * 
 * This test checks that the hash table correctly returns a list of all 
 * the values stored in it.
 */
void test_hash_table_values(void);

/**
 * @brief Tests clearing the hash table.
 * 
 * This test ensures that the hash table can be cleared, removing all 
 * key-value pairs while keeping the table intact.
 */
void test_hash_table_clear(void);

/**
 * @brief Tests the load factor of the hash table.
 * 
 * This test checks if the hash table maintains an appropriate load factor 
 * during operations such as insertions and deletions.
 */
void test_hash_table_load_factor(void);

/**
 * @brief Tests replacing a key-value pair in the hash table.
 * 
 * This test verifies that an existing key can be replaced with a new 
 * value while preserving the integrity of other key-value pairs.
 */
void test_hash_table_replace(void);

/**
 * @brief Tests mapping a function over the hash table.
 * 
 * This test ensures that a function can be applied to each key-value pair 
 * in the hash table, and the results are as expected.
 */
void test_hash_table_map(void);

/**
 * @brief Tests equality of two hash tables.
 * 
 * This test checks if two hash tables are equal, meaning they contain the 
 * same key-value pairs.
 */
void test_hash_table_equals(void);

/**
 * @brief Tests copying a hash table.
 * 
 * This test ensures that a hash table can be copied, and the copy contains 
 * the same key-value pairs as the original.
 */
void test_hash_table_copy(void);

/**
 * @brief Tests merging two hash tables.
 * 
 * This test verifies that two hash tables can be merged correctly, 
 * combining their key-value pairs.
 */
void test_hash_table_merge(void);

#endif // _TEST_HASHTABLE_H
