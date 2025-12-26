/**
 * @file vector.c
 * @brief Implementation of a dynamic array with automatic resizing.
 */

#include "vector/vector.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

// Default initial capacity if not specified
#define VECTOR_DEFAULT_CAPACITY 16
// Growth factor when resizing
#define VECTOR_GROWTH_FACTOR 1.5
// Minimum capacity to avoid too many reallocations for small vectors
#define VECTOR_MIN_CAPACITY 4

// Internal vector structure
struct po_vector {
    void **data;     // Array of void pointers
    size_t size;     // Number of elements currently stored
    size_t capacity; // Current capacity of the array
};

/**
 * @brief Ensure vector has at least min_capacity.
 *
 * Resizes if necessary using growth factor.
 *
 * @param[in] vec Vector handle.
 * @param[in] min_capacity Minimum required capacity.
 * @return 0 on success, -1 on allocation failure.
 *
 * @note Thread-safe: No.
 */
static int vector_ensure_capacity(po_vector_t *vec, size_t min_capacity) {
    if (vec->capacity >= min_capacity)
        return 1; // Already have enough capacity

    // Calculate new capacity with growth factor, avoiding floating-point
    size_t new_capacity = vec->capacity + (vec->capacity >> 1); // Multiply by 1.5
    if (new_capacity < min_capacity)
        new_capacity = min_capacity;

    if (new_capacity < VECTOR_MIN_CAPACITY)
        new_capacity = VECTOR_MIN_CAPACITY;

    // Reallocate memory
    void **new_data = realloc(vec->data, new_capacity * sizeof(void *));
    if (!new_data)
        return -1; // Allocation failed

    vec->data = new_data;
    vec->capacity = new_capacity;
    return 0;
}

po_vector_t *po_vector_create(void) {
    return po_vector_create_sized(VECTOR_DEFAULT_CAPACITY);
}

po_vector_t *po_vector_create_sized(size_t initial_capacity) {
    if (initial_capacity == 0)
        initial_capacity = VECTOR_DEFAULT_CAPACITY;

    po_vector_t *vec = malloc(sizeof(po_vector_t));
    if (!vec)
        return NULL;

    vec->data = malloc(initial_capacity * sizeof(void *));
    if (!vec->data) {
        free(vec);
        return NULL;
    }

    vec->size = 0;
    vec->capacity = initial_capacity;
    return vec;
}

void po_vector_destroy(po_vector_t *restrict vec) {
    if (!vec)
        return;

    free(vec->data);
    free(vec);
}

int po_vector_push(po_vector_t *restrict vec, void *element) {
    if (vec->size >= vec->capacity) {
        int ensure_result = vector_ensure_capacity(vec, vec->capacity + 1);
        if (ensure_result < 0)
            return -1;
    }

    vec->data[vec->size++] = element;
    return 0;
}

void *po_vector_pop(po_vector_t *restrict vec) {
    if (vec->size == 0) {
        errno = EINVAL;
        return NULL;
    }

    return vec->data[--vec->size];
}

void *po_vector_at(const po_vector_t *restrict vec, size_t index) {
    if (index >= vec->size) {
        errno = EINVAL;
        return NULL;
    }

    return vec->data[index];
}

int po_vector_insert(po_vector_t *restrict vec, size_t index, void *element) {
    if (index > vec->size) {
        errno = EINVAL;
        return -1;
    }

    if (vec->size >= vec->capacity) {
        if (vector_ensure_capacity(vec, vec->capacity + 1) != 0) {
            return -1; // Allocation failed
        }
    }

    // Shift elements to make space
    if (index < vec->size) {
        memmove(&vec->data[index + 1], &vec->data[index], (vec->size - index) * sizeof(void *));
    }

    vec->data[index] = element;
    vec->size++;
    return 0;
}

void *po_vector_remove(po_vector_t *restrict vec, size_t index) {
    if (index >= vec->size) {
        errno = EINVAL;
        return NULL;
    }

    void *removed = vec->data[index];

    // Shift elements to fill the gap
    memmove(&vec->data[index], &vec->data[index + 1], (vec->size - index - 1) * sizeof(void *));

    vec->size--;
    return removed;
}

void po_vector_clear(po_vector_t *vec) {
    if (vec)
        vec->size = 0;
}

size_t po_vector_size(const po_vector_t *vec) {
    return vec->size;
}

size_t po_vector_capacity(const po_vector_t *vec) {
    return vec->capacity;
}

int po_vector_reserve(po_vector_t *vec, size_t min_capacity) {
    return vector_ensure_capacity(vec, min_capacity);
}

int po_vector_shrink_to_fit(po_vector_t *vec) {
    if (vec->size == 0) {
        free(vec->data);
        vec->data = NULL;
        vec->capacity = 0;
        return 1;
    }

    if (vec->size < vec->capacity) {
        void **new_data = realloc(vec->data, vec->size * sizeof(void *));
        if (!new_data)
            return -1;

        vec->data = new_data;
        vec->capacity = vec->size;
    }
    return 0;
}

po_vector_t *po_vector_copy(const po_vector_t *src) {
    po_vector_t *dest = po_vector_create_sized(src->capacity);
    if (!dest)
        return NULL;

    memcpy(dest->data, src->data, src->size * sizeof(void *));
    dest->size = src->size;
    return dest;
}

// Iterator implementation
struct po_vector_iter {
    const po_vector_t *vec;
    size_t index;
};

po_vector_iter_t *po_vector_iter_create(const po_vector_t *vec) {
    po_vector_iter_t *iter = malloc(sizeof(po_vector_iter_t));
    if (!iter)
        return NULL;

    iter->vec = vec;
    iter->index = 0;
    return iter;
}

void po_vector_iter_destroy(po_vector_iter_t *iter) {
    free(iter);
}

void *po_vector_next(po_vector_iter_t *iter) {
    if (iter->index >= iter->vec->size)
        return NULL;

    return iter->vec->data[iter->index++];
}

int po_vector_iter_has_next(const po_vector_iter_t *iter) {
    return iter->index < iter->vec->size;
}

int po_vector_is_empty(const po_vector_t *vec) {
    return vec->size == 0;
}

void po_vector_sort(po_vector_t *vec, int (*compare)(const void *, const void *)) {
    if (vec->size > 1) {
        qsort(vec->data, vec->size, sizeof(void *), compare);
    }
}
