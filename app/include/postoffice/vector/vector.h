/**
 * @file vector.h
 * @ingroup vector
 * @brief Dynamic array implementation with automatic resizing.
 *
 * Design Overview
 * ---------------
 *  - Storage: Contiguous array of void pointers for flexible element storage.
 *  - Resizing: Grows by a factor of <code>VECTOR_GROWTH_FACTOR</code> when capacity is reached.
 *    Shrinks when load factor drops below a threshold (optional).
 *  - Memory: Stores raw pointers; does NOT manage the lifetime of pointed-to data.
 *    Callers must manage allocation and deallocation of elements.
 *
 * Big-O Characteristics
 * --------------------
 *  - Access by index: O(1)
 *  - Append (amortized): O(1)
 *  - Insert/remove at arbitrary position: O(n)
 *
 * Memory / Ownership
 * ------------------
 *  - The vector stores raw pointers; it does NOT copy nor free the pointed-to data.
 *  - Callers must ensure the lifetime of inserted elements spans their presence in the vector.
 *  - The vector itself manages the underlying pointer array.
 *
 * Error Handling
 * --------------
 * The vector library uses a consistent error handling strategy:
 * 1. All functions that can fail return an int where:
 *    - 0 indicates success
 *    - -1 indicates failure, with errno set to indicate the specific error
 * 2. Functions that return pointers use NULL to indicate failure, with errno set
 * 3. Error codes used:
 *    - EINVAL: Invalid argument (e.g., NULL vector, out of bounds index)
 *    - ENOMEM: Memory allocation failure
 *    - ENOENT: Element not found (for search operations)
 *
 * Thread Safety
 * -------------
 *  - The vector is not thread-safe by default. If used in a multi-threaded context,
 *    external synchronization is required.
 *
 * @note Initial capacity is 16 by default, grows by 1.5x when full.
 * @see po_vector_push
 * @see po_vector_pop
 */

#ifndef PO_VECTOR_H
#define PO_VECTOR_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque vector handle (contents private to implementation)
typedef struct po_vector po_vector_t;

/**
 * @brief Create a new vector with default initial capacity.
 * @return New vector handle or NULL on allocation failure (errno set).
 * @note Thread-safe: Yes.
 */
po_vector_t *po_vector_create(void) __attribute_malloc__;

/**
 * @brief Create a vector with specified initial capacity.
 * @param[in] initial_capacity Initial capacity of the vector.
 * @return New vector handle or NULL on allocation failure.
 * @note Thread-safe: Yes.
 */
po_vector_t *po_vector_create_sized(size_t initial_capacity) __attribute_malloc__;

/**
 * @brief Free all resources associated with the vector.
 * @param[in] vec Vector to destroy. Does nothing if NULL.
 * @note Thread-safe: No (Must be exclusive).
 */
void po_vector_destroy(po_vector_t *restrict vec);

// Basic operations

/**
 * @brief Append an element to the end of the vector.
 * @param[in] vec Vector handle (must not be NULL).
 * @param[in] element Element to append (pointer value is stored directly).
 * @return 0 on success, -1 on allocation failure.
 * @note Thread-safe: No.
 */
int po_vector_push(po_vector_t *restrict vec, void *element) __nonnull((1));

/**
 * @brief Remove and return the last element in the vector.
 * @param[in] vec Vector handle (must not be NULL).
 * @return The removed element, or NULL if vector is empty.
 * @note Thread-safe: No.
 */
void *po_vector_pop(po_vector_t *restrict vec) __nonnull((1));

/**
 * @brief Get the element at the specified index.
 * @param[in] vec Vector handle (must not be NULL).
 * @param[in] index Zero-based index.
 * @return Element at index, or NULL if out of bounds.
 * @note Thread-safe: Yes (Read-only on vec).
 */
void *po_vector_at(const po_vector_t *restrict vec, size_t index) __nonnull((1));

/**
 * @brief Insert an element at the specified position.
 * @param[in] vec Vector handle (must not be NULL).
 * @param[in] index Position to insert at.
 * @param[in] element Element to insert.
 * @return 0 on success, -1 on allocation failure or invalid index.
 * @note Thread-safe: No.
 */
int po_vector_insert(po_vector_t *restrict vec, size_t index, void *element) __nonnull((1));

/**
 * @brief Replace the element at the specified index.
 * @param[in] vec Vector handle.
 * @param[in] index Position to set.
 * @param[in] element New element.
 * @return 0 on success, -1 if index is out of bounds.
 * @note Thread-safe: No.
 */
int po_vector_set(po_vector_t *vec, size_t index, void *element) __nonnull((1));

/**
 * @brief Remove the element at the specified position.
 * @param[in] vec Vector handle (must not be NULL).
 * @param[in] index Position of element to remove.
 * @return The removed element, or NULL if index is invalid.
 * @note Thread-safe: No.
 */
void *po_vector_remove(po_vector_t *restrict vec, size_t index) __nonnull((1));

// Information

/**
 * @brief Get the number of elements in the vector.
 * @param[in] vec Vector handle.
 * @note Thread-safe: Yes (Read-only).
 */
size_t po_vector_size(const po_vector_t *vec) __nonnull((1));

/**
 * @brief Get the current capacity of the vector.
 * @param[in] vec Vector handle.
 * @note Thread-safe: Yes (Read-only).
 */
size_t po_vector_capacity(const po_vector_t *vec) __nonnull((1));

/**
 * @brief Check if the vector is empty.
 * @param[in] vec Vector handle.
 * @return 1 if empty, 0 otherwise.
 * @note Thread-safe: Yes (Read-only).
 */
int po_vector_is_empty(const po_vector_t *vec) __nonnull((1));

// Memory management

/**
 * @brief Reserve capacity for at least 'capacity' elements.
 * @param[in] vec Vector handle.
 * @param[in] capacity Minimum capacity to reserve.
 * @return 0 on success, -1 on allocation failure.
 * @note Thread-safe: No.
 */
int po_vector_reserve(po_vector_t *vec, size_t capacity) __nonnull((1));

/**
 * @brief Reduce capacity to fit the current size.
 * @param[in] vec Vector handle.
 * @return 0 on success, -1 on allocation failure.
 * @note Thread-safe: No.
 */
int po_vector_shrink_to_fit(po_vector_t *vec) __nonnull((1));

// Advanced operations

/**
 * @brief Sort the vector using the provided comparison function.
 * @param[in,out] vec Vector handle.
 * @param[in] compare Comparison function (compatible with qsort).
 * @note Thread-safe: No.
 */
void po_vector_sort(po_vector_t *vec, int (*compare)(const void *, const void *)) __nonnull((1, 2));

/**
 * @brief Create a shallow copy of the vector.
 * @param[in] vec Vector to copy.
 * @return New vector with same elements, or NULL on allocation failure.
 * @note Thread-safe: Yes (Read-only on source).
 */
po_vector_t *po_vector_copy(const po_vector_t *vec) __nonnull((1)) __attribute_malloc__;

// Iteration

typedef struct po_vector_iter po_vector_iter_t;

/**
 * @brief Initialize an iterator for the vector.
 * @param[in] vec Vector to iterate over (must stay valid during iteration).
 * @return Iterator positioned at the first element, or NULL on error.
 * @note The returned iterator must be freed with po_vector_iter_destroy().
 * @note Thread-safe: Yes.
 */
po_vector_iter_t *po_vector_iter_create(const po_vector_t *vec) __nonnull((1)) __attribute_malloc__;

/**
 * @brief Free resources used by an iterator.
 * @param[in] iter Iterator to destroy. Does nothing if NULL.
 * @note Thread-safe: No (Must be exclusive).
 */
void po_vector_iter_destroy(po_vector_iter_t *iter);

/**
 * @brief Get the next element from the iterator.
 * @param[in] iter Iterator.
 * @return Next element, or NULL if at end or error.
 * @note Thread-safe: No.
 */
void *po_vector_next(po_vector_iter_t *iter) __nonnull((1));

/**
 * @brief Check if the iterator has more elements.
 * @param[in] iter Iterator.
 * @return 1 if more elements are available, 0 otherwise.
 * @note Thread-safe: Yes (Read-only).
 */
int po_vector_iter_has_next(const po_vector_iter_t *iter) __nonnull((1));

#ifdef __cplusplus
}
#endif

#endif // PO_VECTOR_H
