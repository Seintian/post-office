/** \file atomic_queue.h
 *  \ingroup director
 *  \brief Bounded multi-producer single-consumer ring-based queue supplying
 *         the Director's scheduler & task dispatch components with minimal
 *         contention.
 *
 *  Characteristics
 *  ---------------
 *  - Power-of-two capacity ring (similar indexing to perf/ringbuf but with
 *    MPSC semantics).
 *  - Each slot holds a pointer + sequence stamp (ABA avoidance / full-empty
 *    discrimination) enabling wait-free enqueue/dequeue for uncontended paths.
 *  - Producers use atomic compare-exchange loops; consumer advances head with
 *    a monotonic sequence.
 *
 *  Memory Ordering
 *  ---------------
 *  Release when publishing item into slot; acquire when consuming to observe
 *  a fully constructed task pointer.
 *
 *  Backpressure Strategy
 *  ---------------------
 *  When full enqueue returns -1 (errno=EAGAIN) allowing callers to downgrade
 *  or drop low-priority work. Optional spin-yield strategy (future) could be
 *  toggled for latency-sensitive code paths.
 *
 *  Error Handling
 *  --------------
 *  - init: EINVAL (capacity not power-of-two / <2), ENOMEM (allocation fail).
 *  - enqueue: EAGAIN when full.
 *  - dequeue: -1 when empty (errno untouched to keep hot path clean).
 *
 *  Fairness / Starvation
 *  ---------------------
 *  MPSC design can bias towards the fastest producer; batching in the
 *  scheduler mitigates by draining multiple items per tick.
 *
 *  Related
 *  -------
 *  @see task_queue.h higher-level intrusive alternative.
 *  @see scheduler.h consumer that batches queue drains.
 */
#ifndef PO_DIRECTOR_ATOMIC_QUEUE_H
#define PO_DIRECTOR_ATOMIC_QUEUE_H

/* Planned interface:
 *   typedef struct po_atomic_queue po_atomic_queue_t;
 *   int  po_atomic_queue_init(po_atomic_queue_t**, size_t capacity);
 *   void po_atomic_queue_destroy(po_atomic_queue_t*);
 *   int  po_atomic_queue_enqueue(po_atomic_queue_t*, void *ptr);
 *   int  po_atomic_queue_dequeue(po_atomic_queue_t*, void **out);
 *   size_t po_atomic_queue_count(const po_atomic_queue_t*);
 */

#endif /* PO_DIRECTOR_ATOMIC_QUEUE_H */
