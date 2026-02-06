/**
 * @file priority_queue.c
 * @brief Implementation of Indexed Priority Queue.
 *
 * Implements a Min-Heap using a dynamic vector and a hashtable for O(1) index lookups.
 */

#include <errno.h>
#include <postoffice/hashtable/hashtable.h>
#include <postoffice/priority_queue/priority_queue.h>
#include <postoffice/vector/vector.h>
#include <stdint.h>
#include <stdlib.h>

struct po_priority_queue {
    po_vector_t *heap;
    po_hashtable_t *index_map;
    int (*compare)(const void *, const void *);
};

po_priority_queue_t *po_priority_queue_create(int (*compare)(const void *, const void *),
                                              unsigned long (*hash_func)(const void *)) {
    if (!compare || !hash_func) {
        errno = EINVAL;
        return NULL;
    }

    po_priority_queue_t *pq = malloc(sizeof(po_priority_queue_t));
    if (!pq)
        return NULL;

    pq->heap = po_vector_create();
    if (!pq->heap) {
        free(pq);
        return NULL;
    }

    // Pass compare function to hashtable for key equality
    pq->index_map = po_hashtable_create(compare, hash_func);
    if (!pq->index_map) {
        po_vector_destroy(pq->heap);
        free(pq);
        return NULL;
    }

    pq->compare = compare;
    return pq;
}

void po_priority_queue_destroy(po_priority_queue_t *pq) {
    if (!pq)
        return;
    po_hashtable_destroy(&pq->index_map);
    po_vector_destroy(pq->heap);
    free(pq);
}

// Helper: Update index map
static void set_index(po_priority_queue_t *pq, void *element, size_t index) {
    po_hashtable_put(pq->index_map, element, (void *)(intptr_t)index);
}

// Helper: Swap nodes in heap and update map
static void swap_nodes(po_priority_queue_t *pq, size_t i, size_t j) {
    void *elem_i = po_vector_at(pq->heap, i);
    void *elem_j = po_vector_at(pq->heap, j);

    po_vector_set(pq->heap, i, elem_j);
    po_vector_set(pq->heap, j, elem_i); // Now at j is elem_i

    set_index(pq, elem_j, i);
    set_index(pq, elem_i, j);
}

static void sift_up(po_priority_queue_t *pq, size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        void *elem = po_vector_at(pq->heap, index);
        void *p_elem = po_vector_at(pq->heap, parent);

        if (pq->compare(elem, p_elem) < 0) {
            swap_nodes(pq, index, parent);
            index = parent;
        } else {
            break;
        }
    }
}

static void sift_down(po_priority_queue_t *pq, size_t index) {
    size_t size = po_vector_size(pq->heap);
    while (1) {
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;
        size_t smallest = index;

        void *s_elem = po_vector_at(pq->heap, smallest);

        if (left < size) {
            void *l_elem = po_vector_at(pq->heap, left);
            if (pq->compare(l_elem, s_elem) < 0) {
                smallest = left;
                s_elem = l_elem;
            }
        }

        if (right < size) {
            void *r_elem = po_vector_at(pq->heap, right);
            // Compare right with smallest (which might be left or index)
            if (pq->compare(r_elem, s_elem) < 0) {
                smallest = right;
            }
        }

        if (smallest != index) {
            swap_nodes(pq, index, smallest);
            index = smallest;
        } else {
            break;
        }
    }
}

int po_priority_queue_push(po_priority_queue_t *pq, void *element) {
    if (!pq || !element)
        return -1;

    // Check if already exists? Hashtable can tell us.
    if (po_hashtable_contains_key(pq->index_map, element)) {
        errno = EEXIST;
        return -1;
    }

    if (po_vector_push(pq->heap, element) != 0) {
        return -1;
    }

    size_t index = po_vector_size(pq->heap) - 1;
    set_index(pq, element, index);
    sift_up(pq, index);
    return 0;
}

void *po_priority_queue_pop(po_priority_queue_t *pq) {
    if (!pq || po_vector_size(pq->heap) == 0)
        return NULL;

    size_t last_idx = po_vector_size(pq->heap) - 1;
    void *root = po_vector_at(pq->heap, 0);

    if (last_idx == 0) {
        po_vector_pop(pq->heap);
        po_hashtable_remove(pq->index_map, root);
        return root;
    }

    // Move last to root
    swap_nodes(pq, 0, last_idx); // root is now at last_idx

    // Remove last (old root)
    po_vector_pop(pq->heap);
    po_hashtable_remove(pq->index_map, root);

    // Sift down new root
    sift_down(pq, 0);

    return root;
}

void *po_priority_queue_peek(const po_priority_queue_t *pq) {
    if (!pq || po_vector_size(pq->heap) == 0)
        return NULL;
    return po_vector_at(pq->heap, 0);
}

int po_priority_queue_remove(po_priority_queue_t *pq, const void *element) {
    if (!pq || !element)
        return -1;
    if (!po_hashtable_contains_key(pq->index_map, element))
        return -1;

    size_t index = (size_t)(intptr_t)po_hashtable_get(pq->index_map, element);
    size_t last_idx = po_vector_size(pq->heap) - 1;

    if (index == last_idx) {
        po_vector_pop(pq->heap);
        po_hashtable_remove(pq->index_map, element);
        return 0;
    }

    // Swap with last
    void *last_elem = po_vector_at(pq->heap, last_idx);
    swap_nodes(pq, index, last_idx);

    po_vector_pop(pq->heap);
    po_hashtable_remove(pq->index_map, element);

    // Optimization: Check parent
    if (index > 0) {
        size_t parent = (index - 1) / 2;
        void *p_elem = po_vector_at(pq->heap, parent);
        if (pq->compare(last_elem, p_elem) < 0) {
            sift_up(pq, index);
            return 0;
        }
    }

    sift_down(pq, index);
    return 0;
}

size_t po_priority_queue_size(const po_priority_queue_t *pq) {
    return pq ? po_vector_size(pq->heap) : 0;
}

int po_priority_queue_is_empty(const po_priority_queue_t *pq) {
    return pq ? po_vector_is_empty(pq->heap) : 1;
}
