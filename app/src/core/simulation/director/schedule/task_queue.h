/** \file task_queue.h
 *  \ingroup director
 *  \brief Lock-free multi-producer single-consumer (MPSC) intrusive task
 *         queue supporting the Director scheduler.
 *
 *  Purpose
 *  -------
 *  Provide a minimal allocation-free conduit for heterogeneous tasks
 *  (function pointer + context) emitted by various Director subsystems
 *  (IPC bridge, telemetry, runtime state transitions) and executed on the
 *  Director main thread.
 *
 *  Design
 *  ------
 *  - Singly-linked list with atomic head (push) and plain tail (consumer).
 *  - Producers use atomic exchange to insert at head; consumer reverses list
 *    to restore FIFO ordering or performs batch pop with stack behavior if
 *    fairness is not critical (TBD based on workload characteristics).
 *  - Optional fixed-size pool for task node reuse to avoid malloc.
 *
 *  Memory Ordering
 *  ---------------
 *  Producers publish a fully initialized node then atomic-exchange head with
 *  release semantics. Consumer loads head with acquire semantics ensuring
 *  visibility of node contents.
 *
 *  Backpressure
 *  ------------
 *  If a bounded pool is exhausted the enqueue operation fails (-1 / NULL)
 *  allowing callers to decide (drop, retry, escalate). Unbounded mode simply
 *  malloc's (with ENOMEM propagation).
 *
 *  Error Handling
 *  --------------
 *  - Allocation failures set errno=ENOMEM.
 *  - Enqueue on full bounded pool sets errno=EAGAIN.
 *  - Consumer APIs return 0 / -1 for success / empty semantics.
 *
 *  @see scheduler.h for higher-level scheduling semantics.
 */
#ifndef PO_DIRECTOR_TASK_QUEUE_H
#define PO_DIRECTOR_TASK_QUEUE_H

/* Interface under active design; expect functions like:
 *   int po_task_queue_init(po_task_queue_t*, size_t max_nodes);
 *   int po_task_enqueue(po_task_queue_t*, void (*fn)(void*), void *ctx);
 *   size_t po_task_drain(po_task_queue_t*, po_task_record_t *out, size_t max);
 */

#endif /* PO_DIRECTOR_TASK_QUEUE_H */
