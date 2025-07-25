/**
 * @file hashtable.c
 * @brief Implementation of a generic hash table data structure.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "prime/prime.h"
#include "hashtable.h"


// *** MACROS *** //

/**
 * @brief Initial capacity of the hash table.
 * 
 * @note The initial capacity is a prime number to reduce collisions.
 */
#define INITIAL_CAPACITY 17

/** @brief Upper load factor threshold for resizing the table up. */
#define LOAD_FACTOR_UP_THRESHOLD 0.7f

/** @brief Upper load factor tolerance to continue hash_table_put() if hash_table_resize() fails */
#define LOAD_FACTOR_UP_TOLERANCE 1.0f

/** @brief Lower load factor threshold for resizing the table down. */
#define LOAD_FACTOR_DOWN_THRESHOLD 0.2f

// *** STRUCTURES *** //

/**
 * @struct _hash_node_t
 * @brief Node structure to represent an entry in the hash table.
 */
struct _hash_node_t {
    /** @brief Pointer to the key. */
    void* key;

    /** @brief Pointer to the value. */
    void* value;

    /** @brief Pointer to the next node in case of collisions. */
    hash_node_t* next;
} __attribute__((packed));

/**
 * @struct _hash_table_t
 * @brief Structure to represent the hash table.
 */
struct _hash_table_t {
    /** @brief Array of pointers to hash nodes (buckets). */
    hash_node_t** buckets;

    /** @brief The current capacity of the hash table. */
    size_t capacity;

    /** @brief The current number of elements in the hash table. */
    size_t size;

    /** @brief Function pointer for comparing keys. */
    int (*compare)(const void*, const void*);

    /** @brief Function pointer for hashing keys. */
    size_t (*hash_func)(const void*);
} __attribute__((packed));

// *** STATIC *** //

/**
 * @brief Creates a new hash node.
 * 
 * @param[in] key Pointer to the key.
 * @param[in] value Pointer to the value.
 * @return Pointer to the created hash node, or NULL if memory allocation fails.
 * 
 * @note The caller is responsible for freeing the memory allocated for the node.
 */
static hash_node_t* hash_node_create(void* key, void* value) {
    hash_node_t* node = malloc(sizeof(hash_node_t));
    if (!node)
        return NULL;

    node -> key = key;
    node -> value = value;
    node -> next = NULL;

    return node;
}

/**
 * @brief Resizes the hash table to a new capacity.
 * 
 * @param[in] table Pointer to the hash table.
 * @param[in] new_capacity The new capacity to resize to.
 * @return -1 on failure, `RETURN_SUCCESS` on success
 */
static int hash_table_resize(hash_table_t* table, size_t new_capacity) {
    new_capacity = next_prime(new_capacity);

    hash_node_t** new_buckets = calloc(new_capacity, sizeof(hash_node_t*));
    if (!new_buckets)
        return -1;

    for (size_t i = 0; i < table -> capacity; i++) {
        hash_node_t* node = table -> buckets[i];

        while (node) {
            hash_node_t* next_node = node -> next;
            size_t new_hash = table -> hash_func(node -> key);
            size_t new_index = new_hash % new_capacity;

            node -> next = new_buckets[new_index];
            new_buckets[new_index] = node;

            node = next_node;
        }
    }

    free(table -> buckets);
    table -> buckets = new_buckets;
    table -> capacity = new_capacity;

    return 0;
}

// *** API *** //

hash_table_t* hash_table_create_sized(
    int (*compare)(const void*, const void*),
    unsigned long (*hash_func)(const void*),
    size_t base_capacity
) {
    hash_table_t* table = malloc(sizeof(hash_table_t));
    if (!table)
        return NULL;

    table -> capacity = base_capacity;
    table -> size = 0;
    table -> compare = compare;
    table -> hash_func = hash_func;

    table -> buckets = calloc(table -> capacity, sizeof(hash_node_t*));
    if (!table -> buckets) {
        free(table);
        return NULL;
    }

    return table;
}

hash_table_t* hash_table_create(int (*compare)(const void*, const void*), unsigned long (*hash_func)(const void*)) {
    return hash_table_create_sized(compare, hash_func, INITIAL_CAPACITY);
}

// *** Basic hash table operations *** //

int hash_table_put(hash_table_t* table, void* key, void* value) {
    float load_factor = hash_table_load_factor(table);
    if (
        load_factor > LOAD_FACTOR_UP_THRESHOLD
        && hash_table_resize(table, table -> capacity * 2) == -1 
        && load_factor > LOAD_FACTOR_UP_TOLERANCE
    )
        return -1;

    size_t hash = table -> hash_func(key) % table -> capacity;
    hash_node_t* node = table -> buckets[hash];

    while (node) {
        if (table -> compare(node -> key, key) == 0) {
            node -> value = value;
            return 0;
        }

        node = node -> next;
    }

    hash_node_t* new_node = hash_node_create(key, value);
    if (!new_node)
        return -1;

    new_node -> next = table -> buckets[hash];
    table -> buckets[hash] = new_node;
    table -> size++;

    return 1;
}

int hash_table_remove(hash_table_t* table, const void* key) {
    if (
        hash_table_load_factor(table) < LOAD_FACTOR_DOWN_THRESHOLD
        && table -> capacity / 2 >= INITIAL_CAPACITY
        && hash_table_resize(table, table -> capacity / 2) == -1
    )
        return -1;

    size_t hash = table -> hash_func(key);
    size_t index = hash % table -> capacity;
    hash_node_t* node = table -> buckets[index];
    hash_node_t* prev = NULL;

    while (node) {
        if (table -> compare(node -> key, key) == 0) {
            if (prev)
                prev -> next = node -> next;

            else
                table -> buckets[index] = node -> next;

            free(node);
            table -> size--;

            return 1;
        }

        prev = node;
        node = node -> next;
    }

    return 0;
}

void* hash_table_get(const hash_table_t* table, const void* key) {
    size_t hash = table -> hash_func(key);
    size_t index = hash % table -> capacity;
    hash_node_t* node = table -> buckets[index];

    while (node) {
        if (table -> compare(node -> key, key) == 0)
            return node -> value;

        node = node -> next;
    }

    return NULL;
}

int hash_table_contains_key(const hash_table_t* table, const void* key) {
    size_t hash = table -> hash_func(key);
    size_t index = hash % table -> capacity;
    hash_node_t* node = table -> buckets[index];

    while (node) {
        if (table -> compare(node -> key, key) == 0)
            return 1;

        node = node -> next;
    }

    return 0;
}

size_t hash_table_size(const hash_table_t* table) {
    return table -> size;
}

size_t hash_table_capacity(const hash_table_t* table) {
    return table -> capacity;
}

void** hash_table_keyset(const hash_table_t* table) {
    void** keys = calloc(table -> size, sizeof(void*));
    if (!keys)
        return NULL;

    size_t index = 0;
    for (size_t i = 0; i < table -> capacity; i++) {
        hash_node_t* node = table -> buckets[i];

        while (node) {
            keys[index++] = node -> key;
            node = node -> next;
        }
    }

    return keys;
}

int hash_table_clear(hash_table_t* table) {
    if (hash_table_size(table) == 0)
        return 0;

    for (size_t i = 0; i < table -> capacity; i++) {
        hash_node_t* node = table -> buckets[i];

        while (node) {
            hash_node_t* next = node -> next;
            free(node);
            node = next;
        }

        table -> buckets[i] = NULL;
    }

    table -> size = 0;

    return 1;
}

void hash_table_free(hash_table_t* table) {
    hash_table_clear(table);
    free(table -> buckets);
    free(table);
}

// Extended hash table operations

float hash_table_load_factor(const hash_table_t* table) {
    return (float) table -> size / (float) table -> capacity;
}

int hash_table_replace(const hash_table_t* table, const void* key, void* new_value) {
    size_t hash = table -> hash_func(key);
    size_t index = hash % table -> capacity;
    hash_node_t* node = table -> buckets[index];

    while (node) {
        if (table -> compare(node -> key, key) == 0) {
            node -> value = new_value;
            return 1;
        }

        node = node -> next;
    }

    return 0;
}

void hash_table_map(const hash_table_t* table, void (*func)(void* key, void* value)) {
    for (size_t i = 0; i < table -> capacity; i++) {
        hash_node_t* node = table -> buckets[i];

        while (node) {
            func(node -> key, node -> value);
            node = node -> next;
        }
    }
}

void** hash_table_values(const hash_table_t* table) {
    void** values = calloc(table -> size, sizeof(void*));
    if (!values)
        return NULL;

    size_t index = 0;
    for (size_t i = 0; i < table -> capacity; i++) {
        hash_node_t* node = table -> buckets[i];

        while (node) {
            values[index++] = node -> value;
            node = node -> next;
        }
    }

    return values;
}

int hash_table_equals(
    const hash_table_t* table1,
    const hash_table_t* table2,
    int (*compare)(const void*, const void*)
) {
    if (table1 -> size != table2 -> size)
        return 0;

    for (size_t i = 0; i < table1 -> capacity; i++) {
        hash_node_t* node = table1 -> buckets[i];
        hash_node_t* node2 = table2 -> buckets[i];

        while (node && node2) {
            if (table1 -> compare(node -> key, node2 -> key) != 0)
                return 0;

            if (compare(node -> value, node2 -> value) != 0)
                return 0;

            node = node -> next;
            node2 = node2 -> next;
        }
    }

    return 1;
}

void hash_table_merge(hash_table_t* dest, const hash_table_t* source) {
    for (size_t i = 0; i < source -> capacity; i++) {
        hash_node_t* node = source -> buckets[i];

        while (node) {
            hash_table_put(dest, node -> key, node -> value);
            node = node -> next;
        }
    }
}

hash_table_t* hash_table_copy(const hash_table_t* table) {
    hash_table_t* new_table = hash_table_create(table -> compare, table -> hash_func);
    if (!new_table)
        return NULL;

    hash_table_merge(new_table, table);

    return new_table;
}
