#ifndef POSTOFFICE_SORT_H
#define POSTOFFICE_SORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file sort.h
 * @brief Generic thread-safe DriftSort/Timsort implementation.
 *
 * This library implements a highly optimized, adaptive, stable sorting algorithm.
 * It is inspired by DriftSort and Timsort, leveraging natural runs in the data
 * to achieve O(n) performance on nearly sorted inputs, while maintaining uniform
 * O(n log n) worst-case time complexity.
 *
 * Key Features:
 * - **Stable**: Preserves the relative order of equal elements.
 * - **Adaptive**: Exploits existing order (ascending or descending runs).
 * - **Thread-Safe**: Fully re-entrant implementation.
 * - **Memory Efficient**: Uses O(n) auxiliary memory (max n/2).
 * - **Generic**: Works on any data type via void pointers and size.
 */
 
/**
 * @brief Initialize the sort library resources (thread pool).
 *
 * This function should be called once before performing parallel sorts to avoid
 * initialization latency during the first sort call. It initializes the internal
 * thread pool based on available system resources.
 * 
 * Safe to call multiple times (idempotent).
 */
void po_sort_init(void);

/**
 * @brief Release sort library resources.
 *
 * Shuts down the internal thread pool. Should be called when the sort library
 * is no longer needed.
 */
void po_sort_finish(void);

/**
 * @brief Sorts an array of elements using the adaptive FluxSort algorithm.
 *
 * @param base Pointer to the first element of the array.
 * @param nmemb Number of elements in the array.
 * @param size Size in bytes of each element.
 * @param compar Comparison function (returns <0, 0, >0).
 */
void po_sort(void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *));

/**
 * @brief Sorts an array of elements (re-entrant version).
 *
 * This version allows passing a user-defined context argument to the comparison
 * function, which is useful for thread-safe contexts or complex comparisons.
 *
 * @param base Pointer to the first element of the array.
 * @param nmemb Number of elements in the array.
 * @param size Size in bytes of each element.
 * @param compar Comparison function taking an argument.
 * @param arg User-provided argument passed to the comparison function.
 */
void po_sort_r(void *base, size_t nmemb, size_t size,
               int (*compar)(const void *, const void *, void *),
               void *arg);

#ifdef __cplusplus
}
#endif

#endif // POSTOFFICE_SORT_H
