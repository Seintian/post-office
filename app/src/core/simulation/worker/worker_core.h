#ifndef WORKER_CORE_H
#define WORKER_CORE_H

#include <stddef.h>

typedef struct {
    int worker_id;
    int service_type;
    int n_workers;
    char *loglevel;
} worker_config_t;

/**
 * @brief Parse command line arguments into config.
 */
void worker_parse_args(int argc, char **argv, worker_config_t *cfg);

/**
 * @brief Run the worker simulation process.
 * Initializes runtime, spawns workers (multi-threaded or single), and waits for completion.
 * @return 0 on success, non-zero on error.
 */
int worker_run_simulation(const worker_config_t *cfg);

#endif
