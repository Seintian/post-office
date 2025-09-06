#include "postoffice/hashset/hashset.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "prime/prime.h"

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

typedef struct hashset_node {
    /** @brief Pointer to the key. */
    void *key;

    /** @brief Pointer to the next node in case of collisions. */
    struct hashset_node *next;
} __attribute__((packed)) hashset_node_t;

struct po_hashset {
    /** @brief Array of pointers to hash nodes (buckets). */
    hashset_node_t **buckets;

    /** @brief The current capacity of the hash set. */
    size_t capacity;

    /** @brief The current number of elements in the hash set. */
    size_t size;

    /** @brief Function pointer for comparing keys. */
    int (*compare)(const void *, const void *);

    /** @brief Function pointer for hashing keys. */
    size_t (*hash_func)(const void *);
} __attribute__((packed));

// *** STATIC *** //

/**
 * @brief Creates a new hashset node.
 *
 * @param[in] key Pointer to the key.
 * @return Pointer to the created hashset node, or NULL if memory allocation fails.
 *
 * @note The caller is responsible for freeing the memory allocated for the node.
 */
static hashset_node_t *hashset_node_create(void *key) {
    hashset_node_t *node = malloc(sizeof(hashset_node_t));
    if (!node)
        return NULL;

    node->key = key;
    node->next = NULL;

    return node;
}

/**
 * @brief Resizes the hash set to a new capacity.
 *
 * @param[in] set Pointer to the hash set.
 * @param[in] new_capacity New capacity for the hash set.
 * @return 0 on success, -1 on failure.
 */
static int hashset_resize(po_hashset_t *set, size_t new_capacity) {
    new_capacity = next_prime(new_capacity);

    hashset_node_t **new_buckets = calloc(new_capacity, sizeof(hashset_node_t *));
    if (!new_buckets)
        return -1;

    for (size_t i = 0; i < set->capacity; i++) {
        hashset_node_t *node = set->buckets[i];

        while (node) {
            hashset_node_t *next_node = node->next;
            size_t new_hash = set->hash_func(node->key);
            size_t new_index = new_hash % new_capacity;

            node->next = new_buckets[new_index];
            new_buckets[new_index] = node;

            node = next_node;
        }
    }

    free(set->buckets);
    set->buckets = new_buckets;
    set->capacity = new_capacity;

    return 0;
}

// *** API *** //

po_hashset_t *po_hashset_create_sized(int (*compare)(const void *, const void *),
                                      unsigned long (*hash_func)(const void *), size_t initial_capacity) {
    po_hashset_t *set = malloc(sizeof(*set));
    if (!set)
        return NULL;

    set->capacity = next_prime(initial_capacity);
    set->size = 0;
    set->compare = compare;
    set->hash_func = hash_func;

    set->buckets = calloc(set->capacity, sizeof(hashset_node_t *));
    if (!set->buckets) {
        free(set);
        return NULL;
    }

    return set;
}

po_hashset_t *po_hashset_create(int (*compare)(const void *, const void *),
                                unsigned long (*hash_func)(const void *)) {
    return po_hashset_create_sized(compare, hash_func, INITIAL_CAPACITY);
}

// *** Basic hashset operations *** //

int po_hashset_add(po_hashset_t *set, void *key) {
    float load_factor = po_hashset_load_factor(set);
    if (load_factor > LOAD_FACTOR_UP_THRESHOLD && hashset_resize(set, set->capacity * 2) == -1 &&
        load_factor > LOAD_FACTOR_UP_TOLERANCE) {
        return -1;
    }

    size_t hash = set->hash_func(key) % set->capacity;
    hashset_node_t *node = set->buckets[hash];
    while (node) {
        if (set->compare(node->key, key) == 0)
            return 0;

        node = node->next;
    }

    hashset_node_t *new_node = hashset_node_create(key);
    if (!new_node)
        return -1;

    new_node->next = set->buckets[hash];
    set->buckets[hash] = new_node;
    set->size++;

    return 1;
}

int po_hashset_remove(po_hashset_t *set, const void *key) {
    if (set->size == 0)
        return 0;

    size_t hash = set->hash_func(key) % set->capacity;
    hashset_node_t *node = set->buckets[hash];
    hashset_node_t *prev = NULL;

    while (node) {
        if (set->compare(node->key, key) == 0) {
            if (prev)
                prev->next = node->next;

            else
                set->buckets[hash] = node->next;

            free(node);
            set->size--;

            if (po_hashset_load_factor(set) < LOAD_FACTOR_DOWN_THRESHOLD &&
                set->capacity / 2 >= INITIAL_CAPACITY &&
                hashset_resize(set, set->capacity / 2) == -1)
                return -1;

            return 1;
        }

        prev = node;
        node = node->next;
    }

    return 0;
}

int po_hashset_contains(const po_hashset_t *set, const void *key) {
    size_t hash = set->hash_func(key) % set->capacity;
    hashset_node_t *node = set->buckets[hash];

    while (node) {
        if (set->compare(node->key, key) == 0)
            return 1;

        node = node->next;
    }

    return 0;
}

size_t po_hashset_size(const po_hashset_t *set) {
    return set->size;
}

size_t po_hashset_capacity(const po_hashset_t *set) {
    return set->capacity;
}

void **po_hashset_keys(const po_hashset_t *set) {
    if (set->size == 0)
        return NULL;

    void **keys = malloc(set->size * sizeof(void *));
    if (!keys)
        return NULL;

    size_t index = 0;
    for (size_t i = 0; i < set->capacity && index < set->size; i++) {
        hashset_node_t *node = set->buckets[i];

        while (node) {
            keys[index++] = node->key;
            node = node->next;
        }
    }

    return keys;
}

void po_hashset_clear(po_hashset_t *set) {
    if (set->size == 0)
        return;

    for (size_t i = 0; i < set->capacity; i++) {
        hashset_node_t *node = set->buckets[i];
        while (node) {
            hashset_node_t *next = node->next;
            free(node);
            node = next;
        }

        set->buckets[i] = NULL;
    }

    set->size = 0;
}

void po_hashset_destroy(po_hashset_t **set) {
    if (!*set)
        return;

    po_hashset_t *_set = *set;
    if (_set) {
        po_hashset_clear(_set);
        free(_set->buckets);
    }

    free(_set);
    *set = NULL;
}

// Extended hashset operations

float po_hashset_load_factor(const po_hashset_t *set) {
    if (set->capacity == 0)
        return 0.0f;

    return (float)set->size / (float)set->capacity;
}
