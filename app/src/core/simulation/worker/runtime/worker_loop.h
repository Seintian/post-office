#ifndef WORKER_LOOP_H
#define WORKER_LOOP_H

#include <stdbool.h>

/**
 * Initialize global resources for the worker process (logger, metrics, shm).
 * Should be called once before spawning threads.
 */
int worker_global_init(void);

/**
 * Cleanup global resources for the worker process.
 */
void worker_global_cleanup(void);

/**
 * Run the worker loop.
 * @param worker_id Unique ID for this worker (0 to N-1).
 * @param service_type Service type this worker handles.
 */
int worker_run(int worker_id, int service_type);

#endif