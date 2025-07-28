/**
 * @file hashtable.c
 * @brief Implementation of a generic hashtable data structure.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "prime/prime.h"
#include "hashtable/hashtable.h"


// *** MACROS *** //

/**
 * @brief Initial capacity of the hashtable.
 * 
 * @note The initial capacity is a prime number to reduce collisions.
 */
#define INITIAL_CAPACITY 17

/** @brief Upper load factor threshold for resizing the table up. */
#define LOAD_FACTOR_UP_THRESHOLD 0.7f

/** @brief Upper load factor tolerance to continue hashtable_put() if hashtable_resize() fails */
#define LOAD_FACTOR_UP_TOLERANCE 1.0f

/** @brief Lower load factor threshold for resizing the table down. */
#define LOAD_FACTOR_DOWN_THRESHOLD 0.2f

// *** STRUCTURES *** //

/**
 * @struct _hashtable_node_t
 * @brief Node structure to represent an entry in the hashtable.
 */
typedef struct _hashtable_node_t {
    /** @brief Pointer to the key. */
    void *key;

    /** @brief Pointer to the value. */
    void *value;

    /** @brief Pointer to the next node in case of collisions. */
    struct _hashtable_node_t *next;
} __attribute__((packed)) hashtable_node_t;

/**
 * @struct _hashtable_t
 * @brief Structure to represent the hashtable.
 */
struct _hashtable_t {
    /** @brief Array of pointers to hash nodes (buckets). */
    hashtable_node_t **buckets;

    /** @brief The current capacity of the hashtable. */
    size_t capacity;

    /** @brief The current number of elements in the hashtable. */
    size_t size;

    /** @brief Function pointer for comparing keys. */
    int (*compare)(const void *, const void *);

    /** @brief Function pointer for hashing keys. */
    size_t (*hash_func)(const void *);
} __attribute__((packed));

struct _hashtable_iter_t {
    const hashtable_t *table;  ///< Pointer to the hashtable being iterated.
    size_t index;              ///< Current index in the hashtable's bucket array.
    hashtable_node_t *current; ///< Pointer to the current node in the iteration.
};

// *** STATIC *** //

/**
 * @brief Creates a new hashtable node.
 * 
 * @param[in] key Pointer to the key.
 * @param[in] value Pointer to the value.
 * @return Pointer to the created hashtable node, or NULL if memory allocation fails.
 * 
 * @note The caller is responsible for freeing the memory allocated for the node.
 */
static hashtable_node_t *hashtable_node_create(void *key, void *value) {
    hashtable_node_t *node = malloc(sizeof(hashtable_node_t));
    if (!node)
        return NULL;

    node->key = key;
    node->value = value;
    node->next = NULL;

    return node;
}

/**
 * @brief Resizes the hashtable to a new capacity.
 * 
 * @param[in] table Pointer to the hashtable.
 * @param[in] new_capacity The new capacity to resize to.
 * @return -1 on failure, 0 on success
 */
static int hashtable_resize(hashtable_t *table, size_t new_capacity) {
    new_capacity = next_prime(new_capacity);

    hashtable_node_t **new_buckets = calloc(new_capacity, sizeof(hashtable_node_t*));
    if (!new_buckets)
        return -1;

    for (size_t i = 0; i < table->capacity; i++) {
        hashtable_node_t *node = table->buckets[i];

        while (node) {
            hashtable_node_t *next_node = node->next;
            size_t new_hash = table->hash_func(node->key);
            size_t new_index = new_hash % new_capacity;

            node->next = new_buckets[new_index];
            new_buckets[new_index] = node;

            node = next_node;
        }
    }

    free(table->buckets);
    table->buckets = new_buckets;
    table->capacity = new_capacity;

    return 0;
}

// *** API *** //

hashtable_t *hashtable_create_sized(
    int (*compare)(const void *, const void *),
    unsigned long (*hash_func)(const void *),
    size_t base_capacity
) {
    hashtable_t *table = malloc(sizeof(hashtable_t));
    if (!table)
        return NULL;

    table->capacity = base_capacity;
    table->size = 0;
    table->compare = compare;
    table->hash_func = hash_func;

    table->buckets = calloc(table->capacity, sizeof(hashtable_node_t*));
    if (!table->buckets) {
        free(table);
        return NULL;
    }

    return table;
}

hashtable_t *hashtable_create(int (*compare)(const void *, const void *), unsigned long (*hash_func)(const void *)) {
    return hashtable_create_sized(compare, hash_func, INITIAL_CAPACITY);
}

// *** Basic hashtable operations *** //

int hashtable_put(hashtable_t *table, void *key, void *value) {
    float load_factor = hashtable_load_factor(table);
    if (
        load_factor > LOAD_FACTOR_UP_THRESHOLD
        && hashtable_resize(table, table->capacity * 2) == -1 
        && load_factor > LOAD_FACTOR_UP_TOLERANCE
    )
        return -1;

    size_t hash = table->hash_func(key) % table->capacity;
    hashtable_node_t *node = table->buckets[hash];

    while (node) {
        if (table->compare(node->key, key) == 0) {
            node->value = value;
            return 0;
        }

        node = node->next;
    }

    hashtable_node_t *new_node = hashtable_node_create(key, value);
    if (!new_node)
        return -1;

    new_node->next = table->buckets[hash];
    table->buckets[hash] = new_node;
    table->size++;

    return 1;
}

int hashtable_remove(hashtable_t *table, const void *key) {
    if (
        hashtable_load_factor(table) < LOAD_FACTOR_DOWN_THRESHOLD
        && table->capacity / 2 >= INITIAL_CAPACITY
        && hashtable_resize(table, table->capacity / 2) == -1
    )
        return -1;

    size_t hash = table->hash_func(key);
    size_t index = hash % table->capacity;
    hashtable_node_t *node = table->buckets[index];
    hashtable_node_t *prev = NULL;

    while (node) {
        if (table->compare(node->key, key) == 0) {
            if (prev)
                prev->next = node->next;

            else
                table->buckets[index] = node->next;

            free(node);
            table->size--;

            return 1;
        }

        prev = node;
        node = node->next;
    }

    return 0;
}

void *hashtable_get(const hashtable_t *table, const void *key) {
    size_t hash = table->hash_func(key);
    size_t index = hash % table->capacity;
    hashtable_node_t *node = table->buckets[index];

    while (node) {
        if (table->compare(node->key, key) == 0)
            return node->value;

        node = node->next;
    }

    return NULL;
}

int hashtable_contains_key(const hashtable_t *table, const void *key) {
    size_t hash = table->hash_func(key);
    size_t index = hash % table->capacity;
    hashtable_node_t *node = table->buckets[index];

    while (node) {
        if (table->compare(node->key, key) == 0)
            return 1;

        node = node->next;
    }

    return 0;
}

size_t hashtable_size(const hashtable_t *table) {
    return table->size;
}

size_t hashtable_capacity(const hashtable_t *table) {
    return table->capacity;
}

void **hashtable_keyset(const hashtable_t *table) {
    if (table->size == 0)
        return NULL;

    void **keys = calloc(table->size, sizeof(void *));
    if (!keys)
        return NULL;

    size_t index = 0;
    for (size_t i = 0; i < table->capacity && index < table->size; i++) {
        hashtable_node_t *node = table->buckets[i];

        while (node) {
            keys[index++] = node->key;
            node = node->next;
        }
    }

    return keys;
}

int hashtable_clear(hashtable_t *table) {
    if (hashtable_size(table) == 0)
        return 0;

    for (size_t i = 0; i < table->capacity; i++) {
        hashtable_node_t *node = table->buckets[i];

        while (node) {
            hashtable_node_t *next = node->next;
            free(node);
            node = next;
        }

        table->buckets[i] = NULL;
    }

    table->size = 0;

    return 1;
}

void hashtable_free(hashtable_t *table) {
    if (table) {
        hashtable_clear(table);
        free(table->buckets);
    }
    free(table);
}

// Extended hashtable operations

hashtable_iter_t *hashtable_iterator(const hashtable_t *ht) {
    hashtable_iter_t *it = calloc(1, sizeof(hashtable_iter_t));
    if (!it)
        return NULL;

    it->table = ht;

    return it;
}

bool hashtable_iter_next(hashtable_iter_t *it) {
    // If currently in a bucket chain, advance to next node
    if (it->current && it->current->next) {
        it->current = it->current->next;
        return true;
    }

    // Otherwise find next non-empty bucket
    size_t cap = it->table->capacity;
    for (size_t i = it->index; i < cap; i++) {
        hashtable_node_t *n = it->table->buckets[i];
        if (n) {
            it->index = i + 1;
            it->current = n;
            return true;
        }
    }

    return false;
}

void *hashtable_iter_key(const hashtable_iter_t *it) {
    return it->current->key;
}

void *hashtable_iter_value(const hashtable_iter_t *it) {
    return it->current->value;
}

float hashtable_load_factor(const hashtable_t *table) {
    return (float) table->size / (float) table->capacity;
}

int hashtable_replace(const hashtable_t *table, const void *key, void *new_value) {
    size_t hash = table->hash_func(key);
    size_t index = hash % table->capacity;
    hashtable_node_t *node = table->buckets[index];

    while (node) {
        if (table->compare(node->key, key) == 0) {
            node->value = new_value;
            return 1;
        }

        node = node->next;
    }

    return 0;
}

void hashtable_map(const hashtable_t *table, void (*func)(void *key, void *value)) {
    for (size_t i = 0; i < table->capacity; i++) {
        hashtable_node_t *node = table->buckets[i];

        while (node) {
            func(node->key, node->value);
            node = node->next;
        }
    }
}

void **hashtable_values(const hashtable_t *table) {
    void **values = calloc(table->size, sizeof(void *));
    if (!values)
        return NULL;

    size_t index = 0;
    for (size_t i = 0; i < table->capacity; i++) {
        hashtable_node_t *node = table->buckets[i];

        while (node) {
            values[index++] = node->value;
            node = node->next;
        }
    }

    return values;
}

int hashtable_equals(
    const hashtable_t *table1,
    const hashtable_t *table2,
    int (*compare)(const void *, const void *)
) {
    if (table1->size != table2->size)
        return 0;

    for (size_t i = 0; i < table1->capacity; i++) {
        hashtable_node_t *node = table1->buckets[i];
        hashtable_node_t *node2 = table2->buckets[i];

        while (node && node2) {
            if (table1->compare(node->key, node2->key) != 0)
                return 0;

            if (compare(node->value, node2->value) != 0)
                return 0;

            node = node->next;
            node2 = node2->next;
        }
    }

    return 1;
}

void hashtable_merge(hashtable_t *dest, const hashtable_t *source) {
    for (size_t i = 0; i < source->capacity; i++) {
        hashtable_node_t *node = source->buckets[i];

        while (node) {
            hashtable_put(dest, node->key, node->value);
            node = node->next;
        }
    }
}

hashtable_t *hashtable_copy(const hashtable_t *table) {
    hashtable_t *new_table = hashtable_create(table->compare, table->hash_func);
    if (!new_table)
        return NULL;

    hashtable_merge(new_table, table);

    return new_table;
}
