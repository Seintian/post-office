#define _POSIX_C_SOURCE 200809L

#include "runtime/worker_loop.h"
// Actually we need the constant. It's usually in simulation_protocol or ipc.
// Let's assume SIM_MAX_SERVICE_TYPES is available via headers or defined.
// worker_loop.h doesn't expose it. It's in ipc/simulation_protocol.h
#include "../ipc/simulation_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>

typedef struct {
    int id;
    int service_type;
} worker_thread_arg_t;

/**
 * @brief Thread entry point for worker logic.
 * @param[in] arg Pointer to worker_thread_arg_t (takes ownership).
 * @return NULL.
 */
static void* worker_thread_entry(void* arg) {
    worker_thread_arg_t* args = (worker_thread_arg_t*)arg;
    worker_run(args->id, args->service_type);
    free(args);
    return NULL;
}

/**
 * @brief Worker process entry point.
 * @param[in] argc Arg count.
 * @param[in] argv Arg vector.
 * @return 0 on success, >0 on failure.
 */
int main(int argc, char** argv) {
    int worker_id = -1;
    int service_type = -1;
    int n_workers = 0;

    int opt;
    while ((opt = getopt(argc, argv, "l:i:s:w:")) != -1) {
        char *endptr;
        long val;
        switch (opt) {
            case 'l':
                setenv("PO_LOG_LEVEL", optarg, 1);
                break;
            case 'i': 
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val >= 0) worker_id = (int)val;
                break;
            case 's': 
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val >= 0) service_type = (int)val;
                break;
            case 'w':
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val > 0) n_workers = (int)val;
                break;
            default: break;
        }
    }

    // Initialize Global Resources (Shared Memory, Semaphores, Logger)
    if (worker_global_init() != 0) {
        fprintf(stderr, "Failed to initialize worker globals\n");
        return 1;
    }

    if (n_workers > 0) {
        // Multi-threaded mode
        pthread_t* threads = malloc(sizeof(pthread_t) * (size_t)n_workers);
        if (!threads) return 1;

        printf("Spawning %d worker threads...\n", n_workers); // stdout for supervisor info

        for (int i = 0; i < n_workers; i++) {
            worker_thread_arg_t* arg = malloc(sizeof(worker_thread_arg_t));
            arg->id = i;
            arg->service_type = i % SIM_MAX_SERVICE_TYPES; // Round robin assignment
            
            if (pthread_create(&threads[i], NULL, worker_thread_entry, arg) != 0) {
                perror("pthread_create");
            }
        }

        // Wait for all
        for (int i = 0; i < n_workers; i++) {
            pthread_join(threads[i], NULL);
        }
        free(threads);

    } else if (worker_id != -1 && service_type != -1) {
        // Single mode (legacy compatibility)
        worker_run(worker_id, service_type);
    } else {
        fprintf(stderr, "Usage: %s -w <n_workers> OR -i <id> -s <type>\n", argv[0]);
        worker_global_cleanup();
        return 1;
    }

    worker_global_cleanup();
    return 0;
}
