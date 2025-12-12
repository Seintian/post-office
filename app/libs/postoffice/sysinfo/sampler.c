#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "sysinfo/sysinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdatomic.h>

static pthread_t sampler_thread = 0;
static pthread_mutex_t sampler_lock = PTHREAD_MUTEX_INITIALIZER;
static _Atomic(int) sampler_running = 0;
static double cached_cpu_util = -1.0;
static double cached_cpu_iowait = -1.0;

/* Default sampling interval in microseconds (200ms) — reduces sampling overhead
 * while keeping reasonably responsive CPU metrics. */
static unsigned int sampling_interval_us = 200 * 1000;

struct proc_stat_sample {
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long total;
};

static int read_proc_stat(struct proc_stat_sample *s)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
    char *line = NULL;
    size_t n = 0;
    int rc = -1;
    if (getline(&line, &n, f) > 0) {
        // parse the first line: cpu  user nice system idle iowait irq softirq steal guest guest_nice
        unsigned long long user, nice, system, idlev, iowait, irq, softirq, steal, guest, guest_nice;
        int matched = sscanf(line, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                             &user, &nice, &system, &idlev, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);
        if (matched >= 8) {
            unsigned long long non_idle = user + nice + system + irq + softirq + steal;
            unsigned long long total = non_idle + idlev + iowait;
            s->idle = idlev;
            s->iowait = iowait;
            s->total = total;
            rc = 0;
        }
    }
    free(line);
    fclose(f);
    return rc;
}

static void *sampler_loop(void *arg)
{
    (void)arg;
    struct proc_stat_sample a, b;
    while (1) {
        if (!atomic_load_explicit(&sampler_running, memory_order_acquire)) break;
        if (read_proc_stat(&a) != 0) {
            // couldn't read; sleep and retry
            usleep(sampling_interval_us);
            continue;
        }
        usleep(sampling_interval_us);
        if (read_proc_stat(&b) != 0) {
            usleep(sampling_interval_us);
            continue;
        }
        unsigned long long idle_delta = 0, total_delta = 0, iowait_delta = 0;
        if (b.total >= a.total) total_delta = b.total - a.total;
        if (b.idle >= a.idle) idle_delta = b.idle - a.idle;
        if (b.iowait >= a.iowait) iowait_delta = b.iowait - a.iowait;

        double util = -1.0;
        double iow = -1.0;
        if (total_delta > 0) {
            util = (double)(total_delta - idle_delta) * 100.0 / (double)total_delta;
            iow = (double)iowait_delta * 100.0 / (double)total_delta;
        }

        pthread_mutex_lock(&sampler_lock);
        cached_cpu_util = util;
        cached_cpu_iowait = iow;
        pthread_mutex_unlock(&sampler_lock);

        // loop continues
    }
    return NULL;
}

int po_sysinfo_sampler_init(void) {
    if (atomic_load_explicit(&sampler_running, memory_order_acquire)) return 0;
    atomic_store_explicit(&sampler_running, 1, memory_order_release);

    /* Pre-sample once to warm the cache before the background thread starts.
     * This ensures callers get real CPU metrics immediately, not sentinels. */
    struct proc_stat_sample a, b;
    if (read_proc_stat(&a) == 0) {
        usleep(sampling_interval_us);
        if (read_proc_stat(&b) == 0) {
            unsigned long long idle_delta = 0, total_delta = 0, iowait_delta = 0;
            if (b.total >= a.total) total_delta = b.total - a.total;
            if (b.idle >= a.idle) idle_delta = b.idle - a.idle;
            if (b.iowait >= a.iowait) iowait_delta = b.iowait - a.iowait;

            double util = -1.0;
            double iow = -1.0;
            if (total_delta > 0) {
                util = (double)(total_delta - idle_delta) * 100.0 / (double)total_delta;
                iow = (double)iowait_delta * 100.0 / (double)total_delta;
            }

            pthread_mutex_lock(&sampler_lock);
            cached_cpu_util = util;
            cached_cpu_iowait = iow;
            pthread_mutex_unlock(&sampler_lock);
        }
    }

    int rc = pthread_create(&sampler_thread, NULL, sampler_loop, NULL);
    if (rc != 0) {
        atomic_store_explicit(&sampler_running, 0, memory_order_release);
        return -1;
    }
    return 0;
}

void po_sysinfo_sampler_stop(void) {
    if (!atomic_load_explicit(&sampler_running, memory_order_acquire)) return;
    atomic_store_explicit(&sampler_running, 0, memory_order_release);
    if (sampler_thread) pthread_join(sampler_thread, NULL);
    sampler_thread = 0;
    pthread_mutex_lock(&sampler_lock);
    cached_cpu_util = -1.0;
    cached_cpu_iowait = -1.0;
    pthread_mutex_unlock(&sampler_lock);
}

int po_sysinfo_sampler_get(double *cpu_util_pct, double *cpu_iowait_pct) {
    if (!atomic_load_explicit(&sampler_running, memory_order_acquire)) return -1;
    pthread_mutex_lock(&sampler_lock);
    if (cpu_util_pct) *cpu_util_pct = cached_cpu_util;
    if (cpu_iowait_pct) *cpu_iowait_pct = cached_cpu_iowait;
    pthread_mutex_unlock(&sampler_lock);
    return 0;
}
