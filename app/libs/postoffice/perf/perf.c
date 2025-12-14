#include "perf/perf.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <postoffice/log/logger.h>
#include <postoffice/perf/cache.h>
#include <postoffice/hashtable/hashtable.h>

// -----------------------------------------------------------------------------
// Constants & Configuration
// -----------------------------------------------------------------------------
#define SHM_NAME "/postoffice_metrics_shm"
#define MAX_METRIC_NAME 64
#define MAX_COUNTERS 2048
#define MAX_TIMERS 512
#define MAX_HISTOGRAMS 128
#define MAX_HIST_BINS 32

// -----------------------------------------------------------------------------
// Shared Memory Structures
// -----------------------------------------------------------------------------

typedef struct {
    char name[MAX_METRIC_NAME];
    char _pad1[PO_CACHE_LINE_MAX - (MAX_METRIC_NAME % PO_CACHE_LINE_MAX)];

    atomic_uint_fast64_t value;
    char _pad2[PO_CACHE_LINE_MAX - sizeof(atomic_uint_fast64_t)];
} shm_counter_t __attribute__((aligned(PO_CACHE_LINE_MAX)));

typedef struct {
    char name[MAX_METRIC_NAME];
    char _pad1[PO_CACHE_LINE_MAX - (MAX_METRIC_NAME % PO_CACHE_LINE_MAX)];

    struct timespec start; // "Last start" timestamp (caveat: thread-safety on start is caller responsibility)
    char _pad2[PO_CACHE_LINE_MAX - sizeof(struct timespec)];

    atomic_uint_fast64_t total_ns;
    char _pad3[PO_CACHE_LINE_MAX - sizeof(atomic_uint_fast64_t)];
} shm_timer_t __attribute__((aligned(PO_CACHE_LINE_MAX)));

typedef struct {
    char name[MAX_METRIC_NAME];
    char _pad1[PO_CACHE_LINE_MAX - (MAX_METRIC_NAME % PO_CACHE_LINE_MAX)];
    
    size_t nbins;
    uint64_t bins[MAX_HIST_BINS];
    atomic_uint_fast64_t counts[MAX_HIST_BINS];
    
    // Pad entire structure to cache line boundary
    char _pad2[PO_CACHE_LINE_MAX - 
               ((sizeof(size_t) + sizeof(uint64_t) * MAX_HIST_BINS + 
                 sizeof(atomic_uint_fast64_t) * MAX_HIST_BINS) % PO_CACHE_LINE_MAX)];
} shm_histogram_t __attribute__((aligned(PO_CACHE_LINE_MAX)));

typedef struct {
    pthread_mutex_t lock; // Protects allocation/registration
    bool initialized;     // True if the creator has finished initializing

    atomic_size_t num_counters;
    shm_counter_t counters[MAX_COUNTERS];

    atomic_size_t num_timers;
    shm_timer_t timers[MAX_TIMERS];

    atomic_size_t num_histograms;
    shm_histogram_t histograms[MAX_HISTOGRAMS];
} perf_shm_t;

// -----------------------------------------------------------------------------
// Hash Table Helpers
// -----------------------------------------------------------------------------

// String comparison for hash table
static int str_compare(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

// DJB2 hash function for strings
static unsigned long str_hash(const void *key) {
    const char *str = (const char *)key;
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + (unsigned long)c; // hash * 33 + c
    }
    return hash;
}

// -----------------------------------------------------------------------------
// Global Context
// -----------------------------------------------------------------------------
static struct {
    int shm_fd;
    perf_shm_t *shm; // Mapped pointer
    bool is_initialized;
    bool is_creator;

    // Hash tables for O(1) metric lookup (process-local)
    po_hashtable_t *counter_map;   // name -> index
    po_hashtable_t *timer_map;     // name -> index
    po_hashtable_t *histogram_map; // name -> index
    
    // RW lock for thread-safe hash table access
    // Multiple threads can read concurrently, only one can write
    pthread_rwlock_t hash_rwlock;
} ctx = { 
    .shm_fd = -1, 
    .shm = NULL, 
    .is_initialized = false, 
    .is_creator = false,
    .counter_map = NULL,
    .timer_map = NULL,
    .histogram_map = NULL,
    .hash_rwlock = PTHREAD_RWLOCK_INITIALIZER
};

// -----------------------------------------------------------------------------
// Internal Helpers
// -----------------------------------------------------------------------------

static int find_or_alloc_counter(const char *name) {
    if (!ctx.shm || !ctx.counter_map) return -1;
    
    // 1. Fast path: Hash table lookup with read lock (allows concurrent reads)
    pthread_rwlock_rdlock(&ctx.hash_rwlock);
    void *cached = po_hashtable_get(ctx.counter_map, name);
    pthread_rwlock_unlock(&ctx.hash_rwlock);
    
    if (cached) {
        return (int)(intptr_t)cached - 1; // Stored as index+1 to avoid NULL
    }

    // 2. Slow path: Allocate new (with write lock for hash table + SHM lock)
    pthread_rwlock_wrlock(&ctx.hash_rwlock);
    
    // Double-check after acquiring write lock (another thread may have added it)
    cached = po_hashtable_get(ctx.counter_map, name);
    if (cached) {
        pthread_rwlock_unlock(&ctx.hash_rwlock);
        return (int)(intptr_t)cached - 1;
    }
    
    // Now acquire SHM lock for allocation
    pthread_mutex_lock(&ctx.shm->lock);
    
    size_t count = atomic_load(&ctx.shm->num_counters);
    if (count >= MAX_COUNTERS) {
        pthread_mutex_unlock(&ctx.shm->lock);
        pthread_rwlock_unlock(&ctx.hash_rwlock);
        return -1;
    }

    // Initialize new slot
    shm_counter_t *c = &ctx.shm->counters[count];
    strncpy(c->name, name, MAX_METRIC_NAME - 1);
    c->name[MAX_METRIC_NAME - 1] = '\0';
    atomic_init(&c->value, 0);

    // Store in hash table (already have write lock)
    po_hashtable_put(ctx.counter_map, c->name, (void *)(intptr_t)(count + 1));
    
    // Commit
    atomic_store(&ctx.shm->num_counters, count + 1);
    
    pthread_mutex_unlock(&ctx.shm->lock);
    pthread_rwlock_unlock(&ctx.hash_rwlock);
    return (int)count;
}

static int find_or_alloc_timer(const char *name) {
    if (!ctx.shm || !ctx.timer_map) return -1;
    
    // 1. Fast path: Hash table lookup with read lock
    pthread_rwlock_rdlock(&ctx.hash_rwlock);
    void *cached = po_hashtable_get(ctx.timer_map, name);
    pthread_rwlock_unlock(&ctx.hash_rwlock);
    
    if (cached) {
        return (int)(intptr_t)cached - 1;
    }

    // 2. Slow path: Allocate new (with write lock)
    pthread_rwlock_wrlock(&ctx.hash_rwlock);
    
    // Double-check
    cached = po_hashtable_get(ctx.timer_map, name);
    if (cached) {
        pthread_rwlock_unlock(&ctx.hash_rwlock);
        return (int)(intptr_t)cached - 1;
    }
    
    pthread_mutex_lock(&ctx.shm->lock);
    
    size_t count = atomic_load(&ctx.shm->num_timers);
    if (count >= MAX_TIMERS) {
        pthread_mutex_unlock(&ctx.shm->lock);
        pthread_rwlock_unlock(&ctx.hash_rwlock);
        return -1;
    }

    shm_timer_t *t = &ctx.shm->timers[count];
    strncpy(t->name, name, MAX_METRIC_NAME - 1);
    t->name[MAX_METRIC_NAME - 1] = '\0';
    atomic_init(&t->total_ns, 0);
    
    // Store in hash table
    po_hashtable_put(ctx.timer_map, t->name, (void *)(intptr_t)(count + 1));

    atomic_store(&ctx.shm->num_timers, count + 1);
    pthread_mutex_unlock(&ctx.shm->lock);
    pthread_rwlock_unlock(&ctx.hash_rwlock);
    return (int)count;
}

static int get_histogram_index(const char *name) {
    if (!ctx.shm || !ctx.histogram_map) return -1;
    
    // Read lock for hash table lookup
    pthread_rwlock_rdlock(&ctx.hash_rwlock);
    void *cached = po_hashtable_get(ctx.histogram_map, name);
    pthread_rwlock_unlock(&ctx.hash_rwlock);
    
    if (cached) {
        return (int)(intptr_t)cached - 1;
    }
    return -1;
}

// -----------------------------------------------------------------------------
// Public Lookup Wrappers (for macro caching)
// -----------------------------------------------------------------------------

int po_perf_counter_lookup(const char *name) {
    return find_or_alloc_counter(name);
}

int po_perf_timer_lookup(const char *name) {
    return find_or_alloc_timer(name);
}

int po_perf_histogram_lookup(const char *name) {
    return get_histogram_index(name);
}

// -----------------------------------------------------------------------------
// Internal Fast-Path Functions (operate on indices, no lookup)
// -----------------------------------------------------------------------------

void po_perf_counter_inc_by_idx(int idx) {
    if (idx >= 0 && ctx.shm) {
        atomic_fetch_add(&ctx.shm->counters[idx].value, 1);
    }
}

void po_perf_counter_add_by_idx(int idx, uint64_t delta) {
    if (idx >= 0 && ctx.shm) {
        atomic_fetch_add(&ctx.shm->counters[idx].value, delta);
    }
}

// Define clock ID if not available (for inline functions)
#ifndef CLOCK_MONOTONIC_COARSE
    #define CLOCK_MONOTONIC_COARSE CLOCK_MONOTONIC
#endif

void po_perf_timer_start_by_idx(int idx) {
    if (idx >= 0 && ctx.shm) {
        if (clock_gettime(CLOCK_MONOTONIC_COARSE, &ctx.shm->timers[idx].start) != 0) {
            clock_gettime(CLOCK_MONOTONIC, &ctx.shm->timers[idx].start);
        }
    }
}

void po_perf_timer_stop_by_idx(int idx) {
    if (idx >= 0 && ctx.shm) {
        struct timespec end;
        if (clock_gettime(CLOCK_MONOTONIC_COARSE, &end) != 0) {
            clock_gettime(CLOCK_MONOTONIC, &end);
        }
        struct timespec start = ctx.shm->timers[idx].start;
        uint64_t diff = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL +
                        (uint64_t)(end.tv_nsec - start.tv_nsec);
        atomic_fetch_add(&ctx.shm->timers[idx].total_ns, diff);
    }
}

void po_perf_histogram_record_by_idx(int idx, uint64_t value) {
    if (idx >= 0 && ctx.shm) {
        shm_histogram_t *h = &ctx.shm->histograms[idx];
        for (size_t i = 0; i < h->nbins; i++) {
            if (value <= h->bins[i]) {
                atomic_fetch_add(&h->counts[i], 1);
                return;
            }
        }
        atomic_fetch_add(&h->counts[h->nbins], 1);
    }
}

// -----------------------------------------------------------------------------
// Public Initialization
// -----------------------------------------------------------------------------

int po_perf_init(size_t expected_counters, size_t expected_timers, size_t expected_histograms) {
    (void)expected_counters; // Ignored: using fixed SHM size
    (void)expected_timers;
    (void)expected_histograms;

    if (ctx.is_initialized) return 0; // Already initialized in this process

    // Open with create permission (always)
    ctx.shm_fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0666);
    if (ctx.shm_fd < 0) {
        return -1;
    }

    // Try to acquire exclusive lock to determine ownership (Creator)
    bool is_creator = false;
    if (flock(ctx.shm_fd, LOCK_EX | LOCK_NB) == 0) {
        is_creator = true;
    } else {
        if (errno != EWOULDBLOCK) {
            // Unexpected locking error
            close(ctx.shm_fd);
            ctx.shm_fd = -1;
            return -1;
        }
        // EWOULDBLOCK means someone else holds the lock -> We are Attacher
    }

    if (is_creator) {
        // Set size & Reset memory
        if (ftruncate(ctx.shm_fd, sizeof(perf_shm_t)) == -1) {
            if (is_creator) shm_unlink(SHM_NAME); // Clean up if we own it
            close(ctx.shm_fd);
            ctx.shm_fd = -1;
            return -1;
        }
        
        // Map as creator
        ctx.shm = mmap(NULL, sizeof(perf_shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, ctx.shm_fd, 0);
        if (ctx.shm == MAP_FAILED) {
            shm_unlink(SHM_NAME);
            close(ctx.shm_fd);
            ctx.shm_fd = -1;
            return -1;
        }

        // Zero out memory to ensure fresh start (handle stale data from crash)
        memset(ctx.shm, 0, sizeof(perf_shm_t));

        // Init process-shared mutex
        pthread_mutexattr_t mattr;
        pthread_mutexattr_init(&mattr);
        pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&ctx.shm->lock, &mattr);
        pthread_mutexattr_destroy(&mattr);

        atomic_init(&ctx.shm->num_counters, 0);
        atomic_init(&ctx.shm->num_timers, 0);
        atomic_init(&ctx.shm->num_histograms, 0);

        // Mark as initialized so attachers can proceed
        ctx.shm->initialized = true; 
    } else {
        // Attacher path
        ctx.shm = mmap(NULL, sizeof(perf_shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, ctx.shm_fd, 0);
        if (ctx.shm == MAP_FAILED) {
            close(ctx.shm_fd);
            ctx.shm_fd = -1;
            return -1;
        }

        // Wait for creator to finish init
        int spins = 0;
        while (!ctx.shm->initialized) {
            usleep(1000);
            if (++spins > 1000) { // 1 sec timeout
                errno = ETIMEDOUT; // Connection timed out
                return -1;
            }
        }
    }

    // Initialize process-local hash tables for fast metric lookup
    ctx.counter_map = po_hashtable_create_sized(str_compare, str_hash, (size_t)expected_counters);
    ctx.timer_map = po_hashtable_create_sized(str_compare, str_hash, (size_t)expected_timers);
    ctx.histogram_map = po_hashtable_create_sized(str_compare, str_hash, (size_t)expected_histograms);

    if (!ctx.counter_map || !ctx.timer_map || !ctx.histogram_map) {
        // Cleanup on failure
        if (ctx.counter_map) po_hashtable_destroy(&ctx.counter_map);
        if (ctx.timer_map) po_hashtable_destroy(&ctx.timer_map);
        if (ctx.histogram_map) po_hashtable_destroy(&ctx.histogram_map);
        munmap(ctx.shm, sizeof(perf_shm_t));
        close(ctx.shm_fd);
        ctx.shm = NULL;
        ctx.shm_fd = -1;
        return -1;
    }

    ctx.is_initialized = true;
    ctx.is_creator = is_creator;
    return 0;
}

void po_perf_shutdown(FILE *out) {
    if (!ctx.is_initialized) return;

    if (out) {
        po_perf_report(out);
    }

    // Cleanup hash tables
    if (ctx.counter_map) po_hashtable_destroy(&ctx.counter_map);
    if (ctx.timer_map) po_hashtable_destroy(&ctx.timer_map);
    if (ctx.histogram_map) po_hashtable_destroy(&ctx.histogram_map);
    
    // Destroy RW lock
    pthread_rwlock_destroy(&ctx.hash_rwlock);

    // We do NOT unmap SHM here, because the logger worker thread might still be running
    // and processing the logs generated by po_perf_report above. If we unmap now,
    // the worker will crash when trying to increment 'logger.processed' or similar metrics.
    // The OS will clean up the mapping on process exit.

    // Only the creator should unlink the SHM object name to allow for cleanup.
    // If running in a multi-process scenario (Main + Director), Main is usually creators.
    if (ctx.is_creator) {
        shm_unlink(SHM_NAME);
    }

    ctx.is_initialized = false;
    ctx.shm = NULL;
    // Keep fd open or close it? Close is likely safe as mmap persists? 
    // Mmap refs the file description. Closing fd is safe.
    if (ctx.shm_fd >= 0) {
        close(ctx.shm_fd);
        ctx.shm_fd = -1;
    }
}

// -----------------------------------------------------------------------------
// API Implementation
// -----------------------------------------------------------------------------

int po_perf_counter_create(const char *name) {
    if (!ctx.is_initialized) { errno = PERF_ENOTINIT; return -1; }
    if (strlen(name) >= MAX_METRIC_NAME) { errno = EINVAL; return -1; }
    return find_or_alloc_counter(name) >= 0 ? 0 : -1;
}

void po_perf_counter_inc(const char *name) {
    if (!ctx.is_initialized) return;
    int idx = find_or_alloc_counter(name);
    if (idx >= 0) {
        atomic_fetch_add(&ctx.shm->counters[idx].value, 1);
    }
}

void po_perf_counter_add(const char *name, uint64_t delta) {
    if (!ctx.is_initialized) return;
    int idx = find_or_alloc_counter(name);
    if (idx >= 0) {
        atomic_fetch_add(&ctx.shm->counters[idx].value, delta);
    }
}

int po_perf_timer_create(const char *name) {
    if (!ctx.is_initialized) { errno = PERF_ENOTINIT; return -1; }
    if (strlen(name) >= MAX_METRIC_NAME) { errno = EINVAL; return -1; }
    return find_or_alloc_timer(name) >= 0 ? 0 : -1;
}


// Define clock ID if not available
#ifndef CLOCK_MONOTONIC_COARSE
    #define CLOCK_MONOTONIC_COARSE CLOCK_MONOTONIC
#endif

int po_perf_timer_start(const char *name) {
    if (!ctx.is_initialized) { errno = PERF_ENOTINIT; return -1; }
    int idx = find_or_alloc_timer(name);
    if (idx < 0) return -1;

    // Use COARSE clock for performance (lower precision, much faster)
    if (clock_gettime(CLOCK_MONOTONIC_COARSE, &ctx.shm->timers[idx].start) != 0) {
        // Fallback if COARSE is not supported (e.g. older kernels)
        clock_gettime(CLOCK_MONOTONIC, &ctx.shm->timers[idx].start);
    }
    return 0;
}

int po_perf_timer_stop(const char *name) {
    if (!ctx.is_initialized) { errno = PERF_ENOTINIT; return -1; }
    int idx = find_or_alloc_timer(name);
    if (idx < 0) return -1;

    struct timespec end;
    if (clock_gettime(CLOCK_MONOTONIC_COARSE, &end) != 0) {
        clock_gettime(CLOCK_MONOTONIC, &end);
    }

    struct timespec start = ctx.shm->timers[idx].start;
    uint64_t diff = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL +
                    (uint64_t)(end.tv_nsec - start.tv_nsec);

    atomic_fetch_add(&ctx.shm->timers[idx].total_ns, diff);
    return 0;
}


int po_perf_histogram_create(const char *name, const uint64_t *bins, size_t nbins) {
    if (!ctx.is_initialized) { errno = PERF_ENOTINIT; return -1; }
    if (nbins > MAX_HIST_BINS || nbins == 0) { errno = EINVAL; return -1; }
    if (strlen(name) >= MAX_METRIC_NAME) { errno = EINVAL; return -1; }

    // Acquire write lock for hash table + SHM lock for allocation
    pthread_rwlock_wrlock(&ctx.hash_rwlock);
    
    // Check existing (already have lock, call hash table directly)
    void *cached = po_hashtable_get(ctx.histogram_map, name);
    if (cached) {
        pthread_rwlock_unlock(&ctx.hash_rwlock);
        errno = EEXIST;
        return -1;
    }
    
    pthread_mutex_lock(&ctx.shm->lock);

    size_t count = atomic_load(&ctx.shm->num_histograms);
    if (count >= MAX_HISTOGRAMS) {
        pthread_mutex_unlock(&ctx.shm->lock);
        pthread_rwlock_unlock(&ctx.hash_rwlock);
        errno = ENOMEM;
        return -1;
    }

    shm_histogram_t *h = &ctx.shm->histograms[count];
    strncpy(h->name, name, MAX_METRIC_NAME - 1);
    h->name[MAX_METRIC_NAME - 1] = '\0';
    h->nbins = nbins;
    memcpy(h->bins, bins, nbins * sizeof(uint64_t));
    for (size_t i = 0; i < MAX_HIST_BINS; i++) atomic_init(&h->counts[i], 0);

    // Store in hash table
    po_hashtable_put(ctx.histogram_map, h->name, (void *)(intptr_t)(count + 1));

    atomic_store(&ctx.shm->num_histograms, count + 1);
    pthread_mutex_unlock(&ctx.shm->lock);
    pthread_rwlock_unlock(&ctx.hash_rwlock);
    return 0;
}

int po_perf_histogram_record(const char *name, uint64_t value) {
    if (!ctx.is_initialized) return -1;

    int idx = get_histogram_index(name);
    if (idx < 0) return -1;

    shm_histogram_t *h = &ctx.shm->histograms[idx];

    // Linear scan buckets
    for (size_t i = 0; i < h->nbins; i++) {
        if (value <= h->bins[i]) {
            atomic_fetch_add(&h->counts[i], 1);
            return 0;
        }
    }
    // Last bucket
    atomic_fetch_add(&h->counts[h->nbins - 1], 1);
    return 0;
}

int po_perf_flush(void) {
    // No-op for SHM as updates are immediate
    return 0;
}

// -----------------------------------------------------------------------------
// Reporting Helper
// -----------------------------------------------------------------------------

// Helper to compare counters for qsort
static int compare_shm_counters(const void *a, const void *b) {
    const shm_counter_t *ca = a;
    const shm_counter_t *cb = b;
    return strcmp(ca->name, cb->name);
}
static int compare_shm_timers(const void *a, const void *b) {
    const shm_timer_t *ta = a;
    const shm_timer_t *tb = b;
    return strcmp(ta->name, tb->name);
}
static int compare_shm_histograms(const void *a, const void *b) {
    const shm_histogram_t *ha = a;
    const shm_histogram_t *hb = b;
    return strcmp(ha->name, hb->name);
}

static void print_line(FILE *out, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (out) {
        vfprintf(out, fmt, args);
        fprintf(out, "\n");
    } else {
        // Need buffer for LOG_INFO
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, args);
        LOG_INFO("%s", buf);
    }
    va_end(args);
}

int po_perf_report(FILE *out) {
    if (!ctx.is_initialized) return -1;

    print_line(out, "=== Performance Report (SHM) ===");

    // Copy snapshots to sort
    size_t nc = atomic_load(&ctx.shm->num_counters);
    if (nc > 0) {
        print_line(out, "-- Counters --");
        shm_counter_t *local_c = malloc(nc * sizeof(shm_counter_t));
        memcpy(local_c, ctx.shm->counters, nc * sizeof(shm_counter_t));
        qsort(local_c, nc, sizeof(shm_counter_t), compare_shm_counters);
        
        for (size_t i = 0; i < nc; i++) {
             print_line(out, "%s: %lu", local_c[i].name, (unsigned long)atomic_load(&local_c[i].value));
        }
        free(local_c);
    } else {
        print_line(out, "-- Counters -- (none)");
    }

    size_t nt = atomic_load(&ctx.shm->num_timers);
    if (nt > 0) {
        print_line(out, "-- Timers --");
        shm_timer_t *local_t = malloc(nt * sizeof(shm_timer_t));
        memcpy(local_t, ctx.shm->timers, nt * sizeof(shm_timer_t));
        qsort(local_t, nt, sizeof(shm_timer_t), compare_shm_timers);

        for (size_t i = 0; i < nt; i++) {
             print_line(out, "%s: %lu ns", local_t[i].name, (unsigned long)atomic_load(&local_t[i].total_ns));
        }
        free(local_t);
    } else {
        print_line(out, "-- Timers -- (none)");
    }

    size_t nh = atomic_load(&ctx.shm->num_histograms);
    if (nh > 0) {
        print_line(out, "-- Histograms --");
        shm_histogram_t *local_h = malloc(nh * sizeof(shm_histogram_t));
        memcpy(local_h, ctx.shm->histograms, nh * sizeof(shm_histogram_t));
        qsort(local_h, nh, sizeof(shm_histogram_t), compare_shm_histograms);

        for (size_t i = 0; i < nh; i++) {
            print_line(out, "%s:", local_h[i].name);
            for (size_t b = 0; b < local_h[i].nbins; b++) {
                print_line(out, "  <= %lu: %lu", (unsigned long)local_h[i].bins[b], 
                           (unsigned long)atomic_load(&local_h[i].counts[b]));
            }
        }
        free(local_h);
    } else {
        print_line(out, "-- Histograms -- (none)");
    }

    return 0;
}
