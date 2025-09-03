#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "log/logger.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "perf/ringbuf.h"
#include "utils/errors.h"

#define MAX_RECORD_SIZE LOGGER_MSG_MAX * 2

// Runtime level, exported for inline check in header
volatile int _logger_runtime_level = LOG_INFO;

// Record stored in the ring (fixed size, no heap on hot path)
typedef struct log_record {
    struct timespec ts; // high-res timestamp
    uint64_t tid;       // thread id
    uint8_t level;      // logger_level_t
    char file[64];      // truncated
    char func[32];      // truncated
    int line;
    char msg[LOGGER_MSG_MAX]; // truncated fmt result
} log_record_t;

// Ring and worker state
static perf_ringbuf_t *g_ring = NULL; // queue of ready records
static perf_ringbuf_t *g_free = NULL; // freelist of available records
static pthread_t *g_workers = NULL;
static unsigned g_nworkers = 0;
static logger_overflow_policy g_policy = LOGGER_OVERWRITE_OLDEST;
static unsigned int g_sinks_mask = 0u;
static FILE *g_fp = NULL; // file sink
static bool g_console_stderr = true;
static _Atomic int g_running = 0;
static _Atomic unsigned long g_dropped_new = 0;
static _Atomic unsigned long g_overwritten_old = 0;

// Preallocated pool
static log_record_t *g_pool = NULL;
static size_t g_pool_n = 0;

// --- Helpers ---
static inline uint64_t get_tid(void) {
    return (uint64_t)syscall(SYS_gettid);
}

static void record_format_line(const log_record_t *r, char *out, size_t outsz) {
    // Format: ts.us TID LEVEL file:line func() - msg\n
    static const char *lvlstr[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    struct tm tm;
    char tsbuf[32];

    time_t secs = r->ts.tv_sec;
    localtime_r(&secs, &tm);
    strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%d %H:%M:%S", &tm);

    // microseconds
    long usec = r->ts.tv_nsec / 1000L;

    snprintf(out, outsz, "%s.%06ld %llu %-5s %s:%d %s() - %s\n", tsbuf, usec,
             (unsigned long long)r->tid, lvlstr[r->level], r->file, r->line, r->func, r->msg);
}

static void write_record(const log_record_t *r) {
    char line[MAX_RECORD_SIZE];
    record_format_line(r, line, sizeof(line));

    if ((g_sinks_mask & LOGGER_SINK_CONSOLE)) {
        FILE *out = g_console_stderr ? stderr : stdout;
        fputs(line, out);
    }
    if ((g_sinks_mask & LOGGER_SINK_FILE) && g_fp) {
        fputs(line, g_fp);
    }
    // syslog could be added here if linked; omitted for simplicity
}

static void *worker_main(void *arg) {
    (void)arg;
    while (atomic_load_explicit(&g_running, memory_order_relaxed)) {
        void *p = NULL;
        if (perf_ringbuf_dequeue(g_ring, &p) == 0) {
            log_record_t *r = (log_record_t *)p;
            write_record(r);
            // recycle back to freelist
            (void)perf_ringbuf_enqueue(g_free, r);
        } else {
            // no work: sleep a bit to reduce busy-wait
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 2000000}; // 2ms
            nanosleep(&ts, NULL);
        }
    }
    // drain remaining
    void *p;
    while (perf_ringbuf_dequeue(g_ring, &p) == 0) {
        log_record_t *r = (log_record_t *)p;
        write_record(r);
        (void)perf_ringbuf_enqueue(g_free, r);
    }
    return NULL;
}

// --- Public API ---
int logger_init(const logger_config *cfg) {
    if (!cfg || cfg->ring_capacity < 2 || (cfg->ring_capacity & (cfg->ring_capacity - 1))) {
        errno = EINVAL;
        return -1;
    }
    _logger_runtime_level = cfg->level;
    g_policy = cfg->policy;
    g_nworkers = cfg->consumers ? cfg->consumers : 1;

    // reset sinks and counters for a clean start
    g_sinks_mask = 0u;
    if (g_fp) {
        fclose(g_fp);
    }
    g_fp = NULL;
    atomic_store_explicit(&g_dropped_new, 0ul, memory_order_relaxed);
    atomic_store_explicit(&g_overwritten_old, 0ul, memory_order_relaxed);

    perf_ringbuf_set_cacheline(64);
    g_ring = perf_ringbuf_create(cfg->ring_capacity);
    if (!g_ring)
        return -1;

    // preallocate pool and freelist (use same capacity for simplicity)
    g_pool_n = cfg->ring_capacity; // number of records
    g_pool = (log_record_t *)calloc(g_pool_n, sizeof(*g_pool));
    if (!g_pool) {
        perf_ringbuf_destroy(&g_ring);
        return -1;
    }
    g_free = perf_ringbuf_create(cfg->ring_capacity);
    if (!g_free) {
        free(g_pool);
        g_pool = NULL;
        g_pool_n = 0;
        perf_ringbuf_destroy(&g_ring);
        return -1;
    }
    for (size_t i = 0; i < g_pool_n; i++) {
        (void)perf_ringbuf_enqueue(g_free, &g_pool[i]);
    }

    g_workers = calloc(g_nworkers, sizeof(*g_workers));
    if (!g_workers) {
        perf_ringbuf_destroy(&g_ring);
        return -1;
    }

    atomic_store_explicit(&g_running, 1, memory_order_relaxed);
    for (unsigned i = 0; i < g_nworkers; i++)
        pthread_create(&g_workers[i], NULL, worker_main, NULL);

    return 0;
}

void logger_shutdown(void) {
    if (!g_ring)
        return;
    atomic_store_explicit(&g_running, 0, memory_order_relaxed);
    for (unsigned i = 0; i < g_nworkers; i++)
        pthread_join(g_workers[i], NULL);

    free(g_workers);
    g_workers = NULL;
    g_nworkers = 0;
    perf_ringbuf_destroy(&g_ring);
    perf_ringbuf_destroy(&g_free);
    free(g_pool);
    g_pool = NULL;
    g_pool_n = 0;

    if (g_fp) {
        fflush(g_fp);
        fclose(g_fp);
        g_fp = NULL;
    }
}

int logger_set_level(logger_level_t level) {
    if (level < LOG_TRACE || level > LOG_FATAL) {
        errno = EINVAL;
        return -1;
    }
    _logger_runtime_level = level;
    return 0;
}

logger_level_t logger_get_level(void) {
    return (logger_level_t)_logger_runtime_level;
}

int logger_add_sink_console(bool use_stderr) {
    g_sinks_mask |= LOGGER_SINK_CONSOLE;
    g_console_stderr = use_stderr;
    return 0;
}

int logger_add_sink_file(const char *path, bool append) {
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    const char *mode = append ? "a" : "w";
    FILE *fp = fopen(path, mode);
    if (!fp)
        return -1;
    g_fp = fp;
    g_sinks_mask |= LOGGER_SINK_FILE;
    return 0;
}

int logger_add_sink_syslog(const char *ident) {
    (void)ident; // placeholder without linking syslog
    g_sinks_mask |= LOGGER_SINK_SYSLOG;
    return 0;
}

static void enqueue_record(log_record_t *rec) {
    // Try to enqueue; if full, apply policy
    if (perf_ringbuf_enqueue(g_ring, rec) == 0)
        return;

    if (g_policy == LOGGER_DROP_NEW) {
        // drop the newly created rec
        (void)atomic_fetch_add_explicit(&g_dropped_new, 1, memory_order_relaxed);
        // return back to freelist
        (void)perf_ringbuf_enqueue(g_free, rec);
        return;
    }
    // Overwrite oldest: drop one from head, then enqueue
    void *old;
    if (perf_ringbuf_dequeue(g_ring, &old) == 0) {
        (void)atomic_fetch_add_explicit(&g_overwritten_old, 1, memory_order_relaxed);
        (void)perf_ringbuf_enqueue(g_free, (log_record_t *)old);
    }
    (void)perf_ringbuf_enqueue(g_ring, rec); // best-effort
}

// Emit an internal warning if many logs were lost. We try to rate-limit by
// only emitting when counters cross a power-of-two boundary, to avoid floods
// during sustained overload.
static inline int should_emit_overflow_notice(unsigned long v) {
    return (v != 0ul) && ((v & (v - 1ul)) == 0ul); // power of two
}

void logger_logv(logger_level_t level, const char *file, int line, const char *func,
                 const char *fmt, va_list ap) {
    if (!g_ring)
        return; // not initialized yet

    // obtain a record from freelist (non-blocking)
    void *ptr = NULL;
    if (perf_ringbuf_dequeue(g_free, &ptr) != 0) {
        // no free slots: effectively dropping the new message
        unsigned long dropped =
            atomic_fetch_add_explicit(&g_dropped_new, 1, memory_order_relaxed) + 1ul;
        // Best-effort: emit a synchronous overflow notice when hitting
        // a power-of-two threshold to ensure visibility even under
        // sustained freelist exhaustion.
        if (should_emit_overflow_notice(dropped)) {
            log_record_t w2;
            clock_gettime(CLOCK_REALTIME, &w2.ts);
            w2.tid = get_tid();
            w2.level = (uint8_t)LOG_ERROR;
            w2.line = 0;
            w2.file[0] = '\0';
            strncpy(w2.func, "logger", sizeof(w2.func) - 1);
            w2.func[sizeof(w2.func) - 1] = '\0';
            unsigned long overwritten =
                atomic_load_explicit(&g_overwritten_old, memory_order_relaxed);
            snprintf(w2.msg, sizeof(w2.msg),
                     "logger overflow: dropped_new=%lu overwritten_old=%lu (policy=%s)", dropped,
                     overwritten, g_policy == LOGGER_DROP_NEW ? "DROP_NEW" : "OVERWRITE_OLDEST");
            write_record(&w2);
        }
        return;
    }
    log_record_t *r = (log_record_t *)ptr;

    clock_gettime(CLOCK_REALTIME, &r->ts);
    r->tid = get_tid();
    r->level = (uint8_t)level;
    r->line = line;

    // Copy file/func with truncation
    if (file) {
        size_t n = strnlen(file, sizeof(r->file) - 1);
        memcpy(r->file, file + (n >= sizeof(r->file) ? n - (sizeof(r->file) - 1) : 0),
               n >= sizeof(r->file) ? sizeof(r->file) - 1 : n);
        r->file[sizeof(r->file) - 1] = '\0';
    } else {
        r->file[0] = '\0';
    }
    if (func) {
        strncpy(r->func, func, sizeof(r->func) - 1);
        r->func[sizeof(r->func) - 1] = '\0';
    } else {
        r->func[0] = '\0';
    }

    // Format message into fixed buffer
    vsnprintf(r->msg, sizeof(r->msg), fmt ? fmt : "", ap);

    enqueue_record(r);

    // After enqueue, check loss counters and emit a summary warning
    unsigned long dropped = atomic_load_explicit(&g_dropped_new, memory_order_relaxed);
    unsigned long overwritten = atomic_load_explicit(&g_overwritten_old, memory_order_relaxed);
    if (should_emit_overflow_notice(dropped) || should_emit_overflow_notice(overwritten)) {
        // craft a synthetic record (best-effort) to inform about loss
        void *p2 = NULL;
        if (perf_ringbuf_dequeue(g_free, &p2) == 0) {
            log_record_t *w = (log_record_t *)p2;
            clock_gettime(CLOCK_REALTIME, &w->ts);
            w->tid = get_tid();
            w->level = (uint8_t)LOG_ERROR;
            w->line = 0;
            w->file[0] = '\0';
            strncpy(w->func, "logger", sizeof(w->func) - 1);
            w->func[sizeof(w->func) - 1] = '\0';
            snprintf(w->msg, sizeof(w->msg),
                     "logger overflow: dropped_new=%lu overwritten_old=%lu (policy=%s)", dropped,
                     overwritten, g_policy == LOGGER_DROP_NEW ? "DROP_NEW" : "OVERWRITE_OLDEST");
            if (perf_ringbuf_enqueue(g_ring, w) != 0) {
                // Ring still full: write synchronously and recycle
                write_record(w);
                (void)perf_ringbuf_enqueue(g_free, w);
            }
        } else {
            // No freelist entry available: emit synchronously to sinks to ensure visibility
            log_record_t w2;
            clock_gettime(CLOCK_REALTIME, &w2.ts);
            w2.tid = get_tid();
            w2.level = (uint8_t)LOG_ERROR;
            w2.line = 0;
            w2.file[0] = '\0';
            strncpy(w2.func, "logger", sizeof(w2.func) - 1);
            w2.func[sizeof(w2.func) - 1] = '\0';
            snprintf(w2.msg, sizeof(w2.msg),
                     "logger overflow: dropped_new=%lu overwritten_old=%lu (policy=%s)", dropped,
                     overwritten, g_policy == LOGGER_DROP_NEW ? "DROP_NEW" : "OVERWRITE_OLDEST");
            write_record(&w2);
        }
    }
}

void logger_log(logger_level_t level, const char *file, int line, const char *func, const char *fmt,
                ...) {
    if ((level < (logger_level_t)LOGGER_COMPILE_LEVEL) ||
        (level < (logger_level_t)_logger_runtime_level))
        return;
    va_list ap;
    va_start(ap, fmt);
    logger_logv(level, file, line, func, fmt, ap);
    va_end(ap);
}
