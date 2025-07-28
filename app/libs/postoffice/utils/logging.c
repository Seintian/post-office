#include "utils/logging.h"
#include "utils/errors.h"
#include "perf/perf.h"
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>


#define DEFAULT_LOG_LEVEL LOG_INFO

typedef struct _logging_ctx_t {
    size_t num_producers;
    size_t queue_size;
    int global_level;
    sem_t wakeup;
    pthread_t consumer_tid;
    // ringbuf_t **queues;
    pthread_key_t producer_key;
    // pthread_mutex_t shutdown_lock;
    // bool shutdown;
} logging_ctx_t;

static int global_level = DEFAULT_LOG_LEVEL;

int logging_init(size_t threads, size_t queue_size, int level) {
    if (level < LOG_TRACE || level > LOG_FATAL) {
        errno = LOG_EINVAL;
        return -1;
    }

    global_level = level;

    (void)threads; // Placeholder for future use
    (void)queue_size; // Placeholder for future use

    return 0;
}

void logging_shutdown(void) {
    // Placeholder for future use
    // This function should clean up any resources allocated during logging_init()
    // such as freeing queues, destroying semaphores, etc.
}

int logging_add_file(FILE *fp, int level) {
    if (level < LOG_TRACE || level > LOG_FATAL) {
        errno = LOG_EINVAL;
        return -1;
    }

    // Placeholder for future use
    (void)fp; // File pointer to log file

    return 0;
}

int logging_add_callback(log_LogFn fn, void *udata, int level) {
    if (level < LOG_TRACE || level > LOG_FATAL) {
        errno = LOG_EINVAL;
        return -1;
    }

    // Placeholder for future use
    (void)fn; // Function pointer to callback
    (void)udata; // User data for callback

    return 0;
}

int logging_set_level(int level) {
    if (level < LOG_TRACE || level > LOG_FATAL) {
        errno = LOG_EINVAL;
        return -1;
    }

    global_level = level;
    return 0;
}

bool logging_level_enabled(int level) {
    return level >= global_level;
}

void logging_enqueue(
    int level,
    const char *file,
    int line,
    const char *fmt,
    ...
) {
    (void)level; // Placeholder for future use
    (void)file; // Placeholder for future use
    (void)line; // Placeholder for future use
    (void)fmt; // Placeholder for future use
}
