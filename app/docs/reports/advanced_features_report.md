# Professional Technical Report: Advanced Orchestration Features

## Executive Summary

This report outlines the technical design and implementation of advanced features added to the Post Office simulation project. These enhancements improve the system's resilience, fairness, and resource efficiency, transforming it from a sequential task processor into a dynamic, priority-aware orchestrator.

## 1. Dynamic Load Balancing (DLB)

The simulation now features a **Dynamic Load Balancing** module integrated into the central Director process. This module addresses the problem of worker starvation and queue bottlenecks in a multi-process environment.

### Technical Implementation

The DLB system monitors the `waiting_count` of each service queue in shared memory. When the ratio between the most overloaded and the least loaded queue exceeds a predefined threshold (e.g., 2.0 or 200%), the Director intervenes.

```c
// load_balance.c: Reassignment Logic
if (ratio >= g_config.imbalance_threshold) {
    int worker_idx = find_idle_worker_for_service(shm, underloaded, n_workers);
    if (worker_idx >= 0) {
        atomic_store(&shm->workers[worker_idx].service_type, overloaded);
        atomic_store(&shm->workers[worker_idx].reassignment_pending, 1);
        // Wake up workers on the new queue
        pthread_cond_broadcast(&shm->queues[overloaded].cond_added);
    }
}
```

### Rationale
By reassigning idle workers from under-utilized services to over-burdened ones, the system significantly reduces the "Simulation-wide average waiting time" and prevents the `SIM_ENDED_EXPLODED` termination condition.

---

## 2. Priority-Based Task Scheduling (Work Broker)

The legacy Ticket Issuer has been superseded by a multi-threaded **Work Broker**. This component introduces the concept of **Tiered Quality of Service (QoS)** through VIP user simulation.

### Implementation Details
The Work Broker utilizes a custom-built **Indexed Min-Heap (Priority Queue)** to manage pending requests. This data structure ensures that tasks are distributed based on a multi-factor priority:
1.  **VIP Status**: Users designated as VIPs (10% random chance) bypass the standard FIFO order.
2.  **Arrival Time (FIFO)**: Within the same priority tier, requests are handled by age.

```c
// work_broker.c: Priority Comparison
static int item_compare(const void *a, const void *b) {
    const broker_item_t *ia = (const broker_item_t *)a;
    const broker_item_t *ib = (const broker_item_t *)b;

    if (ia->is_vip != ib->is_vip) {
        return ib->is_vip - ia->is_vip; // Higher priority for VIPs
    }
    return (int)(ia->arrival_time.tv_sec - ib->arrival_time.tv_sec); // FIFO fallback
}
```

---

## 3. High-Performance Data Structures

To support $O(\log N)$ scheduling operations, a specialized **Indexed Priority Queue** was developed. This library combines the performance of a binary heap with the fast lookup capabilities of a hashtable.

### Key Benefits
- **Logarithmic Scaling**: Even with thousands of concurrent users, task assignment remains efficient.
- **Fast Arbitrary Removal**: Enables the system to remove specific items from the queue (e.g., if a user leaves or a process fails) in $O(\log N)$ time, which is impossible with standard heaps ($O(N)$).

---

## Conclusion

The integration of these advanced features—Dynamic Load Balancing, Priority-aware Brokerage, and Optimized Scheduling Structures—elevates the Post Office simulation to a professional-grade demonstration of distributed system principles. These changes are fully documented and integrated into the core architectural specifications.
