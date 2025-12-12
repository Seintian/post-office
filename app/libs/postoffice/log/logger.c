// Ensure feature test macros for syscall and related POSIX/GNU extensions.
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// For __nonnull attribute
#include <sys/cdefs.h>

#include "log/logger.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "metrics/metrics.h"
#include "perf/batcher.h"
#include "perf/ringbuf.h"

#define MAX_RECORD_SIZE LOGGER_MSG_MAX * 2

// Runtime level, exported for inline check in header
volatile po_log_level_t _logger_runtime_level = LOG_INFO;

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

typedef struct custom_sink {
    void (*fn)(const char *, void *);
    void *ud;
    struct custom_sink *next;
} custom_sink_t;

// Ring and worker state
static po_perf_ringbuf_t *g_ring = NULL;    // queue of ready records
static po_perf_ringbuf_t *g_free = NULL;    // freelist of available records
static po_perf_batcher_t *g_batcher = NULL; // eventfd-based batching
static pthread_t *g_workers = NULL;
static unsigned g_nworkers = 0;
static po_logger_overflow_policy_t g_policy = LOGGER_OVERWRITE_OLDEST;
static unsigned int g_sinks_mask = 0u;
static FILE *g_fp = NULL; // file sink
static bool g_console_stderr = true;
static _Atomic int g_running = 0;
static _Atomic unsigned long g_dropped_new = 0;
static _Atomic unsigned long g_overwritten_old = 0;
static _Atomic unsigned long g_processed = 0;
static _Atomic unsigned long g_batches = 0;
static _Atomic unsigned long g_max_batch = 0; // high water mark
static int g_syslog_open = 0;
static char *g_syslog_ident = NULL;
static custom_sink_t *g_custom_sinks = NULL;

// Preallocated pool
static log_record_t *g_pool = NULL;
static size_t g_pool_n = 0;
static log_record_t g_sentinel; // shutdown marker (never in freelist)

static const char *const level_metrics[] = {"logger.level.trace", "logger.level.debug",
                                            "logger.level.info",  "logger.level.warn",
                                            "logger.level.error", "logger.level.fatal"};

// --- Directory Helper ---
static int mkdir_p(const char *path) {
    if (!path || !*path) return -1;
    char *subpath = strdup(path);
    if (!subpath) return -1;

    char *p = subpath;
    // Skip leading slash
    if (*p == '/') p++;

    for (; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(subpath, 0755) != 0) {
                if (errno != EEXIST) {
                    free(subpath);
                    return -1;
                }
            }
            *p = '/';
        }
    }

    // Create the final leaf if it's a directory path (this function expects a directory path)
    if (mkdir(subpath, 0755) != 0) {
        if (errno != EEXIST) {
            free(subpath);
            return -1;
        }
    }

    free(subpath);
    return 0;
}

// --- Helpers ---
static inline uint64_t get_tid(void) {
    return (uint64_t)syscall(SYS_gettid);
}

static inline void recycle_record(log_record_t *r) {
    (void)perf_ringbuf_enqueue(g_free, r);
}

static void record_format_line(const log_record_t *r, char *out, size_t outsz) {
    static const char *lvlstr[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    struct tm tm;
    char tsbuf[32];

    time_t secs = r->ts.tv_sec;
    localtime_r(&secs, &tm);
    strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%d %H:%M:%S", &tm);

    long usec = r->ts.tv_nsec / 1000L; // microseconds
    snprintf(out, outsz, "%s.%06ld %lu %-5s %s:%d %s() - %s\n", tsbuf, usec, r->tid,
             lvlstr[r->level], r->file, r->line, r->func, r->msg);
}

static inline int syslog_priority_for_level(uint8_t lvl) {
    switch (lvl) {
    case LOG_FATAL:
        return LOG_CRIT;

    case LOG_ERROR:
        return LOG_ERR;

    case LOG_WARN:
        return LOG_WARNING;

    case LOG_INFO:
        return LOG_INFO;

    case LOG_DEBUG: // fallthrough
    case LOG_TRACE:
    default:
        return LOG_DEBUG;
    }
}

static void write_record(const log_record_t *r) {
    char line[MAX_RECORD_SIZE];
    record_format_line(r, line, sizeof(line));

    if (g_sinks_mask & LOGGER_SINK_CONSOLE) {
        FILE *out = g_console_stderr ? stderr : stdout;
        fputs(line, out);
    }

    if ((g_sinks_mask & LOGGER_SINK_FILE) && g_fp)
        fputs(line, g_fp);

    if ((g_sinks_mask & LOGGER_SINK_SYSLOG) && g_syslog_open)
        syslog(syslog_priority_for_level(r->level), "%s:%d %s() - %s", r->file, r->line, r->func,
               r->msg);

    for (custom_sink_t *c = g_custom_sinks; c; c = c->next)
        c->fn(line, c->ud);
}

static void *worker_main(void *arg) {
    (void)arg;

    if (!g_batcher)
        return NULL;

    const size_t max_batch = 256;
    void **batch = calloc(max_batch, sizeof(void *));
    if (!batch)
        return NULL;

    while (atomic_load_explicit(&g_running, memory_order_relaxed)) {
        ssize_t n = perf_batcher_next(g_batcher, batch);
        if (n <= 0) {
            if (!atomic_load_explicit(&g_running, memory_order_relaxed))
                break;
            continue; // spurious wake
        }

        PO_METRIC_COUNTER_INC("logger.batch.count");
        PO_METRIC_COUNTER_ADD("logger.batch.records", n);
        atomic_fetch_add_explicit(&g_batches, 1, memory_order_relaxed);
        unsigned long prev = atomic_load_explicit(&g_max_batch, memory_order_relaxed);
        if ((unsigned long)n > prev)
            atomic_compare_exchange_strong(&g_max_batch, &prev, (unsigned long)n);

        for (ssize_t i = 0; i < n; ++i) {
            log_record_t *r = (log_record_t *)batch[i];
            if (r == &g_sentinel)
                continue; // shutdown wake

            write_record(r);
            PO_METRIC_COUNTER_INC("logger.processed");

            if (r->level <= LOG_FATAL)
                PO_METRIC_COUNTER_INC(level_metrics[r->level]);

            atomic_fetch_add_explicit(&g_processed, 1, memory_order_relaxed);
            recycle_record(r);
        }

        // Batch processed, flush file sink if active
        if ((g_sinks_mask & LOGGER_SINK_FILE) && g_fp)
            fflush(g_fp);
    }

    void *p; // Drain remaining
    while (perf_ringbuf_dequeue(g_ring, &p) == 0) {
        log_record_t *r = (log_record_t *)p;
        if (r != &g_sentinel) {
            write_record(r);
            PO_METRIC_COUNTER_INC("logger.processed");

            if (r->level <= LOG_FATAL)
                PO_METRIC_COUNTER_INC(level_metrics[r->level]);

            recycle_record(r);
        }
    }

    // Final flush
    if ((g_sinks_mask & LOGGER_SINK_FILE) && g_fp)
        fflush(g_fp);

    free(batch);
    return NULL;
}

// --- Public API ---

int po_logger_init(const po_logger_config_t *cfg) {
    if (cfg->ring_capacity < 2 || (cfg->ring_capacity & (cfg->ring_capacity - 1))) {
        errno = EINVAL;
        return -1;
    }

    int rc_ret = 0;

    _logger_runtime_level = cfg->level;
    g_policy = cfg->policy;
    g_nworkers = cfg->consumers ? cfg->consumers : 1;

    g_sinks_mask = 0u; // reset sinks and counters
    if (g_fp)
        fclose(g_fp);

    g_fp = NULL;
    atomic_store_explicit(&g_dropped_new, 0ul, memory_order_relaxed);
    atomic_store_explicit(&g_overwritten_old, 0ul, memory_order_relaxed);

    size_t cacheline = cfg->cacheline_bytes;
    perf_ringbuf_set_cacheline(cacheline);
    g_ring = perf_ringbuf_create(cfg->ring_capacity);
    if (!g_ring)
        return -1;

    g_pool_n = cfg->ring_capacity;
    g_pool = calloc(g_pool_n, sizeof(*g_pool));
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

    g_batcher = perf_batcher_create(g_ring, 256);
    if (!g_batcher) {
        perf_ringbuf_destroy(&g_free);
        g_free = NULL;
        free(g_pool);
        g_pool = NULL;
        g_pool_n = 0;
        perf_ringbuf_destroy(&g_ring);
        g_ring = NULL;
        return -1;
    }

    for (size_t i = 0; i < g_pool_n; i++)
        perf_ringbuf_enqueue(g_free, &g_pool[i]);

    g_workers = calloc(g_nworkers, sizeof(*g_workers));
    if (!g_workers) {
        perf_batcher_destroy(&g_batcher);
        perf_ringbuf_destroy(&g_free);
        perf_ringbuf_destroy(&g_ring);
        free(g_pool);
        g_pool = NULL;
        g_pool_n = 0;
        return -1;
    }

    atomic_store_explicit(&g_running, 1, memory_order_relaxed);
    for (unsigned i = 0; i < g_nworkers; i++)
        pthread_create(&g_workers[i], NULL, worker_main, NULL);

    PO_METRIC_COUNTER_INC("logger.init");
    return rc_ret;
}

void po_logger_shutdown(void) {
    if (!g_ring)
        return;

    PO_METRIC_COUNTER_INC("logger.shutdown");
    atomic_store_explicit(&g_running, 0, memory_order_relaxed);

    for (unsigned i = 0; i < g_nworkers; i++) {
        if (g_batcher)
            perf_batcher_enqueue(g_batcher, &g_sentinel);
        else
            perf_ringbuf_enqueue(g_ring, &g_sentinel);
    }
    for (unsigned i = 0; i < g_nworkers; i++)
        pthread_join(g_workers[i], NULL);

    free(g_workers);
    g_workers = NULL;
    g_nworkers = 0;
    if (g_batcher)
        perf_batcher_destroy(&g_batcher);

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
    if (g_syslog_open) {
        closelog();
        g_syslog_open = 0;
    }
    if (g_syslog_ident) {
        free(g_syslog_ident);
        g_syslog_ident = NULL;
    }

    custom_sink_t *c = g_custom_sinks;
    while (c) {
        custom_sink_t *n = c->next;
        free(c);
        c = n;
    }

    g_custom_sinks = NULL;
}

int po_logger_set_level(po_log_level_t level) {
    if (level < LOG_TRACE || level > LOG_FATAL) {
        errno = EINVAL;
        return -1;
    }

    _logger_runtime_level = level;
    return 0;
}

po_log_level_t po_logger_get_level(void) {
    return (po_log_level_t)_logger_runtime_level;
}

int po_logger_add_sink_console(bool use_stderr) {
    g_sinks_mask |= LOGGER_SINK_CONSOLE;
    g_console_stderr = use_stderr;
    return 0;
}

int po_logger_add_sink_file(const char *path, bool append) {
    // Attempt to create parent directories
    char *dir = strdup(path);
    if (dir) {
        char *slash = strrchr(dir, '/');
        if (slash) {
            *slash = '\0';
            // Ignore errors here; straightforward fopen will fail if mkdir failed, checking that is enough
            mkdir_p(dir); 
        }
        free(dir);
    }

    const char *mode = append ? "a" : "w";
    FILE *fp = fopen(path, mode);
    if (!fp)
        return -1;

    g_fp = fp;
    g_sinks_mask |= LOGGER_SINK_FILE;
    return 0;
}

int po_logger_add_sink_syslog(const char *ident) {
    if (!g_syslog_open) {
        if (g_syslog_ident) {
            free(g_syslog_ident);
            g_syslog_ident = NULL;
        }

        if (ident && *ident)
            g_syslog_ident = strdup(ident);

        openlog(g_syslog_ident ? g_syslog_ident : "postoffice", LOG_PID | LOG_NDELAY, LOG_USER);
        g_syslog_open = 1;
    }

    g_sinks_mask |= LOGGER_SINK_SYSLOG;
    return 0;
}

int po_logger_add_sink_custom(void (*fn)(const char *line, void *udata), void *udata) {
    custom_sink_t *c = malloc(sizeof(*c));
    if (!c)
        return -1;

    c->fn = fn;
    c->ud = udata;
    c->next = g_custom_sinks;
    g_custom_sinks = c;
    return 0;
}

static void enqueue_record(log_record_t *rec) {
    if (g_batcher) {
        if (perf_batcher_enqueue(g_batcher, rec) == 0) {
            PO_METRIC_COUNTER_INC("logger.enqueue");
            return;
        }
    } else if (perf_ringbuf_enqueue(g_ring, rec) == 0) {
        PO_METRIC_COUNTER_INC("logger.enqueue");
        return;
    }

    if (g_policy == LOGGER_DROP_NEW) {
        PO_METRIC_COUNTER_INC("logger.drop_new");
        atomic_fetch_add_explicit(&g_dropped_new, 1, memory_order_relaxed);
        recycle_record(rec);
        return;
    }

    void *old;
    if (perf_ringbuf_dequeue(g_ring, &old) == 0) {
        PO_METRIC_COUNTER_INC("logger.overwrite_old");
        atomic_fetch_add_explicit(&g_overwritten_old, 1, memory_order_relaxed);
        recycle_record((log_record_t *)old);
    }
    if (g_batcher) {
        if (perf_batcher_enqueue(g_batcher, rec) == 0)
            PO_METRIC_COUNTER_INC("logger.enqueue");
    } else if (perf_ringbuf_enqueue(g_ring, rec) == 0)
        PO_METRIC_COUNTER_INC("logger.enqueue");
}

static inline int should_emit_overflow_notice(unsigned long v) {
    return (v != 0UL) && ((v & (v - 1UL)) == 0UL);
}

static inline void fill_overflow_notice(log_record_t *rec, unsigned long dropped,
                                        unsigned long overwritten) {
    clock_gettime(CLOCK_REALTIME, &rec->ts);
    rec->tid = get_tid();
    rec->level = (uint8_t)LOG_ERROR;
    rec->line = 0;
    rec->file[0] = '\0';
    strncpy(rec->func, "logger", sizeof(rec->func) - 1);
    rec->func[sizeof(rec->func) - 1] = '\0';
    snprintf(rec->msg, sizeof(rec->msg),
             "logger overflow: dropped_new=%lu overwritten_old=%lu (policy=%s)", dropped,
             overwritten, g_policy == LOGGER_DROP_NEW ? "DROP_NEW" : "OVERWRITE_OLDEST");
}

void po_logger_logv(po_log_level_t level, const char *file, int line, const char *func,
                    const char *fmt, va_list ap) {
    if (!g_ring)
        return; // not initialized yet

    void *ptr = NULL; // obtain a record from freelist (non-blocking)
    if (perf_ringbuf_dequeue(g_free, &ptr) != 0) {
        PO_METRIC_COUNTER_INC("logger.no_free_record");

        unsigned long dropped =
            atomic_fetch_add_explicit(&g_dropped_new, 1, memory_order_relaxed) + 1UL;
        if (should_emit_overflow_notice(dropped)) {
            PO_METRIC_COUNTER_INC("logger.overflow.notice");

            log_record_t w2;
            unsigned long overwritten =
                atomic_load_explicit(&g_overwritten_old, memory_order_relaxed);
            fill_overflow_notice(&w2, dropped, overwritten);
            write_record(&w2);
        }

        return;
    }

    log_record_t *r = (log_record_t *)ptr;

    clock_gettime(CLOCK_REALTIME, &r->ts);
    r->tid = get_tid();
    r->level = (uint8_t)level;
    r->line = line;

    if (file) {
        size_t n = strnlen(file, sizeof(r->file) - 1);
        memcpy(r->file, file + (n >= sizeof(r->file) ? n - (sizeof(r->file) - 1) : 0),
               n >= sizeof(r->file) ? sizeof(r->file) - 1 : n);
        r->file[sizeof(r->file) - 1] = '\0';
    } else
        r->file[0] = '\0';

    if (func) {
        strncpy(r->func, func, sizeof(r->func) - 1);
        r->func[sizeof(r->func) - 1] = '\0';
    } else
        r->func[0] = '\0';

    if (fmt && *fmt) {
        va_list ap_copy;
        va_copy(ap_copy, ap);
        vsnprintf(r->msg, sizeof(r->msg), fmt, ap_copy);
        va_end(ap_copy);
    } else {
        r->msg[0] = '\0';
    }

    enqueue_record(r);

    unsigned long dropped = atomic_load_explicit(&g_dropped_new, memory_order_relaxed);
    unsigned long overwritten = atomic_load_explicit(&g_overwritten_old, memory_order_relaxed);
    if (should_emit_overflow_notice(dropped) || should_emit_overflow_notice(overwritten)) {
        PO_METRIC_COUNTER_INC("logger.overflow.notice");

        void *p2 = NULL;
        if (perf_ringbuf_dequeue(g_free, &p2) == 0) {
            log_record_t *w = (log_record_t *)p2;
            fill_overflow_notice(w, dropped, overwritten);
            if (perf_ringbuf_enqueue(g_ring, w) != 0) {
                write_record(w);
                recycle_record(w);
            }
        } else {
            log_record_t w2;
            fill_overflow_notice(&w2, dropped, overwritten);
            write_record(&w2);
        }
    }
}

void po_logger_log(po_log_level_t level, const char *file, int line, const char *func,
                   const char *fmt, ...) {
    if ((level < (po_log_level_t)LOGGER_COMPILE_LEVEL) ||
        (level < (po_log_level_t)_logger_runtime_level))
        return;

    va_list ap;
    va_start(ap, fmt);
    po_logger_logv(level, file, line, func, fmt, ap);
    va_end(ap);
}

void po_logger_crash_dump(int fd) {
    char scratch[MAX_RECORD_SIZE];
    const char *header = "\n--- Pending Log Messages (Ring Buffer Dump) ---\n";
    if (write(fd, header, strlen(header)) < 0) {} 

    // Drain ring buffer
    // Note: This modifies the ring buffer state which is acceptable during a crash.
    void *p;
    while (g_ring && perf_ringbuf_dequeue(g_ring, &p) == 0) {
        log_record_t *r = (log_record_t *)p;
        if (r && r != &g_sentinel) {
            record_format_line(r, scratch, sizeof(scratch));
            if (write(fd, scratch, strlen(scratch)) < 0) {} 
        }
    }
    const char *footer = "--- End of Pending Logs ---\n";
    if (write(fd, footer, strlen(footer)) < 0) {} 
}
