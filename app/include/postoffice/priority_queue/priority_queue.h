/**
 * @file priority_queue.h
 * @ingroup priority_queue
 * @brief Indexed Priority Queue (Min-Heap) implementation.
 *
 * Supports O(log N) insertion, removal, and arbitrary removal by element.
 * Backed by a generic vector and a hashtable for index tracking.
 */

#ifndef PO_PRIORITY_QUEUE_H
#define PO_PRIORITY_QUEUE_H

#include <stddef.h>
#include <postoffice/vector/vector.h>
#include <postoffice/hashtable/hashtable.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct po_priority_queue po_priority_queue_t;

/**
 * @brief Create a new priority queue.
 * @param compare Function to compare two elements. Returns <0 if a < b (Min-Heap property).
 * @param hash_func Hash function for the elements (used to track indices for fast removal).
 * @return New priority queue handle or NULL on failure.
 */
po_priority_queue_t *po_priority_queue_create(int (*compare)(const void *, const void *),
                                              unsigned long (*hash_func)(const void *));

/**
 * @brief Destroy the priority queue.
 * @param pq Pointer to the handle.
 */
void po_priority_queue_destroy(po_priority_queue_t *pq);

/**
 * @brief Push an element into the priority queue.
 * @param pq Queue handle.
 * @param element Element pointer.
 * @return 0 on success, -1 on failure.
 */
int po_priority_queue_push(po_priority_queue_t *pq, void *element);

/**
 * @brief Remove and return the top element (min).
 * @param pq Queue handle.
 * @return Top element or NULL if empty.
 */
void *po_priority_queue_pop(po_priority_queue_t *pq);

/**
 * @brief Peek at the top element without removing.
 * @param pq Queue handle.
 * @return Top element or NULL if empty.
 */
void *po_priority_queue_peek(const po_priority_queue_t *pq);

/**
 * @brief Remove a specific element from the queue.
 * @param pq Queue handle.
 * @param element Element to remove (must compare equal to existing).
 * @return 0 on success, -1 if not found.
 */
int po_priority_queue_remove(po_priority_queue_t *pq, const void *element);

/**
 * @brief Get the number of elements.
 * @param pq Queue handle.
 * @return Size.
 */
size_t po_priority_queue_size(const po_priority_queue_t *pq);

/**
 * @brief Check if empty.
 * @param pq Queue handle.
 * @return 1 if empty, 0 otherwise.
 */
int po_priority_queue_is_empty(const po_priority_queue_t *pq);

#ifdef __cplusplus
}
#endif
#endif // PO_PRIORITY_QUEUE_H
