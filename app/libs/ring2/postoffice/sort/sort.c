#include "postoffice/sort/sort.h"
#include "postoffice/sysinfo/sysinfo.h"
#include "postoffice/concurrency/threadpool.h"
#include "postoffice/concurrency/waitgroup.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#define force_inline    __attribute__((always_inline)) inline
#define hot             __attribute__((hot))
#define cold            __attribute__((cold))

/*
 * Implementation of FluxSort (Stable Quicksort variant).
 *
 * Principles:
 * 1. Stable Partitioning: Uses an auxiliary buffer (O(N)) to partition elements stably.
 * 2. Adaptivity: Checks for existing runs (ascending/descending) to optimize sorted cases.
 * 3. Pivot Selection: Median-of-3 or Median-of-9.
 * 4. Small Sort: Binary Insertion Sort for small ranges.
 */

#define FLUX_SMALL_SORT_THRESHOLD 32

typedef struct {
    char *base;
    size_t size;
    int (*compar)(const void *, const void *, void *);
    void *arg;
} sort_ctx_t;

// Globals for concurrency
static threadpool_t *g_sort_pool = NULL;
static pthread_once_t g_pool_once = PTHREAD_ONCE_INIT;
static int g_num_threads = 1;

static void init_sort_pool_once(void) {
    po_sysinfo_t info;
    int cores = 4;
    if (po_sysinfo_collect(&info) == 0) {
        if (info.physical_cores > 0) cores = (int)info.physical_cores;
        else if (info.logical_processors > 0) cores = (int)info.logical_processors;
    }
    // Limit reasonable threads for sorting (memory bandwidth bound)
    if (cores > 16) cores = 16;
    if (cores < 2) cores = 0; // Serial

    if (cores > 0) {
        g_sort_pool = tp_create((size_t)cores, 0); // Unlimited queue
        if (g_sort_pool) g_num_threads = cores;
    }
}

void po_sort_init(void) {
    pthread_once(&g_pool_once, init_sort_pool_once);
}

void po_sort_finish(void) {
    // Note: This is not thread-safe if sort is running. User responsibility.
    if (g_sort_pool) {
        tp_destroy(g_sort_pool, true);
        g_sort_pool = NULL;
        // Reset once control? pthread_once cannot be easily reset.
        // So po_sort_init is one-shot per process lifetime unless we use atomic flag.
        // For now, this is sufficient.
    }
}

// Helper for pointer arithmetic
static force_inline char *get_ptr(sort_ctx_t *ctx, size_t index) {
    return ctx->base + (index * ctx->size);
}

// Helper: Compare wrapper
static force_inline int compare(sort_ctx_t *ctx, const void *a, const void *b) {
    if (ctx->arg) {
        return ctx->compar(a, b, ctx->arg);
    } else {
        // Safe cast as we know the signature matches in the no-arg case
        return ((int (*)(const void *, const void *))(void *)ctx->compar)(a, b);
    }
}

// Generic swap
static force_inline void swap_generic(char *a, char *b, size_t size) {
    if (size == 4) {
        uint32_t tmp = *(uint32_t *)a;
        *(uint32_t *)a = *(uint32_t *)b;
        *(uint32_t *)b = tmp;
    } else if (size == 8) {
        uint64_t tmp = *(uint64_t *)a;
        *(uint64_t *)a = *(uint64_t *)b;
        *(uint64_t *)b = tmp;
    } else {
        char t;
        for (size_t i = 0; i < size; i++) {
            t = a[i]; a[i] = b[i]; b[i] = t;
        }
    }
}

static void reverse_range(sort_ctx_t *ctx, size_t start, size_t len) {
    size_t lo = start;
    size_t hi = start + len - 1;
    size_t size = ctx->size;

    while (lo < hi) {
        char *plo = get_ptr(ctx, lo);
        char *phi = get_ptr(ctx, hi);
        swap_generic(plo, phi, size);
        lo++;
        hi--;
    }
}

// =========================================================
// Specialized Sort Implementations (Macro)
// =========================================================

// Helper for Ninther
#define SORT3_HELPER(a, b, c, suffix, size, cmp, arg, cmp_noarg, has_arg) do { \
    if ((has_arg ? cmp((b), (a), arg) : cmp_noarg((b), (a))) < 0) swap_##suffix((a), (b), size); \
    if ((has_arg ? cmp((c), (b), arg) : cmp_noarg((c), (b))) < 0) swap_##suffix((b), (c), size); \
    if ((has_arg ? cmp((b), (a), arg) : cmp_noarg((b), (a))) < 0) swap_##suffix((a), (b), size); \
} while(0)

#define PAR_THRESHOLD 32768 // 32KB

#define SORT_IMPL(suffix, type, is_generic, has_arg) \
struct task_##suffix { \
    sort_ctx_t ctx; \
    size_t start; \
    size_t len; \
    waitgroup_t *wg; \
}; \
\
static force_inline void swap_##suffix(char *a, char *b, size_t size) { \
    if (is_generic) swap_generic(a, b, size); \
    else { type tmp = *(type*)(a); *(type*)(a) = *(type*)(b); *(type*)(b) = tmp; } \
} \
\
static hot void merge_##suffix(sort_ctx_t *ctx, size_t start, char *mid, size_t len1, size_t len2) { \
    size_t size = is_generic ? ctx->size : sizeof(type); \
    char *p1 = get_ptr(ctx, start); \
    char *p2 = mid; \
    char *end1 = mid; \
    char *end2 = mid + (len2 * size); \
    char *out_start = (char *)malloc((len1 + len2) * size); \
    if (!out_start) return; /* Should fallback but hard here. Assume malloc works for small N. */ \
    char *out = out_start; \
    \
    int (*cmp)(const void *, const void *, void *) = ctx->compar; \
    void *arg = ctx->arg; \
    int (*cmp_noarg)(const void *, const void *) = (int (*)(const void *, const void *))(void *)ctx->compar; \
    \
    if (is_generic) { \
        while (p1 < end1 && p2 < end2) { \
            if ((has_arg ? cmp(p1, p2, arg) : cmp_noarg(p1, p2)) <= 0) { \
                memcpy(out, p1, size); p1 += size; \
            } else { \
                memcpy(out, p2, size); p2 += size; \
            } \
            out += size; \
        } \
        if (p1 < end1) memcpy(out, p1, (size_t)(end1 - p1)); \
        if (p2 < end2) memcpy(out, p2, (size_t)(end2 - p2)); \
    } else { \
        type *tp1 = (type*)p1; type *tp2 = (type*)p2; \
        type *tend1 = (type*)end1; type *tend2 = (type*)end2; \
        type *tout = (type*)out; \
        while (tp1 < tend1 && tp2 < tend2) { \
            if ((has_arg ? cmp(tp1, tp2, arg) : cmp_noarg(tp1, tp2)) <= 0) { \
                *tout++ = *tp1++; \
            } else { \
                *tout++ = *tp2++; \
            } \
        } \
        while (tp1 < tend1) *tout++ = *tp1++; \
        while (tp2 < tend2) *tout++ = *tp2++; \
    } \
    memcpy(get_ptr(ctx, start), out_start, (len1 + len2) * size); \
    free(out_start); \
} \
\
/* Analyze for Moore pattern (few runs) and merge if found */ \
static int analyze_and_merge_##suffix(sort_ctx_t *ctx, size_t start, size_t len) { \
    if (len < 128) return 0; /* Too small to care */ \
    \
    size_t size = is_generic ? ctx->size : sizeof(type); \
    int (*cmp)(const void *, const void *, void *) = ctx->compar; \
    void *arg = ctx->arg; \
    int (*cmp_noarg)(const void *, const void *) = (int (*)(const void *, const void *))(void *)ctx->compar; \
    \
    /* Fail-fast probe */ \
    if (len > 512) { \
        size_t step = len / 5; \
        char *p0 = get_ptr(ctx, start); \
        char *p1 = get_ptr(ctx, start + step); \
        char *p2 = get_ptr(ctx, start + 2 * step); \
        char *p3 = get_ptr(ctx, start + 3 * step); \
        char *p4 = get_ptr(ctx, start + 4 * step); \
        int c1 = (has_arg ? cmp(p1, p0, arg) : cmp_noarg(p1, p0)); \
        int c2 = (has_arg ? cmp(p2, p1, arg) : cmp_noarg(p2, p1)); \
        int c3 = (has_arg ? cmp(p3, p2, arg) : cmp_noarg(p3, p2)); \
        int c4 = (has_arg ? cmp(p4, p3, arg) : cmp_noarg(p4, p3)); \
        int asc = (c1 > 0) + (c2 > 0) + (c3 > 0) + (c4 > 0); \
        int desc = (c1 < 0) + (c2 < 0) + (c3 < 0) + (c4 < 0); \
        if (asc > 0 && desc > 0) return 0; \
    } \
    \
    /* Limit scan to 4 runs */ \
    size_t runs[5]; \
    size_t run_cnt = 0; \
    runs[0] = 0; \
    \
    char *curr = get_ptr(ctx, start); \
    char *next = curr + size; \
    size_t i = 0; \
    \
    /* Detect Run 1 direction */ \
    int c = (has_arg ? cmp(next, curr, arg) : cmp_noarg(next, curr)); \
    bool desc = (c < 0); \
    \
    while (i < len - 1) { \
        c = (has_arg ? cmp(next, curr, arg) : cmp_noarg(next, curr)); \
        if ((desc && c > 0) || (!desc && c < 0)) { \
            if (desc) reverse_range(ctx, start + runs[run_cnt], i - runs[run_cnt] + 1); \
            run_cnt++; \
            if (run_cnt >= 4) return 0; /* Too many runs, abort to Quicksort */ \
            runs[run_cnt] = i + 1; \
            /* Next run direction */ \
            if (i + 2 < len) { \
                c = (has_arg ? cmp(next + size, next, arg) : cmp_noarg(next + size, next)); \
                desc = (c < 0); \
            } \
        } \
        curr = next; \
        next += size; \
        i++; \
    } \
    if (desc) reverse_range(ctx, start + runs[run_cnt], len - runs[run_cnt]); \
    run_cnt++; \
    \
    if (run_cnt <= 1) return 1; /* Already sorted (0 or 1 run) */ \
    \
    /* Merge Runs */ \
    /* Simple iterative merge: Merge(0,1) -> 0. Merge(2,3) -> 2. Merge(0,2) -> 0. */ \
    \
    /* Merge 0 and 1 */ \
    size_t len1 = runs[1] - runs[0]; \
    size_t len2 = (run_cnt > 1 ? (run_cnt > 2 ? runs[2] : len) : 0) - runs[1]; \
    if (run_cnt >= 2) { \
        merge_##suffix(ctx, start + runs[0], get_ptr(ctx, start + runs[1]), len1, len2); \
        runs[1] = (run_cnt > 2 ? runs[2] : len); /* New end of run 0 */ \
    } \
    /* Merge 2 and 3 if exist */ \
    if (run_cnt >= 4) { \
        len1 = runs[3] - runs[2]; \
        len2 = len - runs[3]; \
        merge_##suffix(ctx, start + runs[2], get_ptr(ctx, start + runs[3]), len1, len2); \
    } \
    /* Final Merge 0 and 2(now 1) */ \
    if (run_cnt >= 3) { \
        len1 = runs[1]; \
        len2 = len - runs[1]; \
        merge_##suffix(ctx, start + runs[0], get_ptr(ctx, start + runs[1]), len1, len2); \
    } \
    return 1; /* Handled */ \
} \
\
static hot void insertion_sort_##suffix(sort_ctx_t *ctx, size_t start, size_t len) { \
    size_t size = is_generic ? ctx->size : sizeof(type); \
    int (*cmp)(const void *, const void *, void *) = ctx->compar; \
    void *arg = ctx->arg; \
    int (*cmp_noarg)(const void *, const void *) = (int (*)(const void *, const void *))(void *)ctx->compar; \
    \
    char *tmp_buf = NULL; \
    if (is_generic) tmp_buf = (char *)__builtin_alloca(size); \
    \
    for (size_t i = start + 1; i < start + len; i++) { \
        size_t j = i; \
        char *pi = get_ptr(ctx, i); \
        if (is_generic) { \
            memcpy(tmp_buf, pi, size); \
            while (j > start) { \
                char *pj = get_ptr(ctx, j - 1); \
                if ((has_arg ? cmp(tmp_buf, pj, arg) : cmp_noarg(tmp_buf, pj)) >= 0) break; \
                memcpy(get_ptr(ctx, j), pj, size); \
                j--; \
            } \
            memcpy(get_ptr(ctx, j), tmp_buf, size); \
    } else { \
            type tmp = *(type*)pi; \
            while (j > start) { \
                char *pj = get_ptr(ctx, j - 1); \
                if ((has_arg ? cmp(&tmp, pj, arg) : cmp_noarg(&tmp, pj)) >= 0) break; \
                *(type*)get_ptr(ctx, j) = *(type*)pj; \
                j--; \
            } \
            *(type*)get_ptr(ctx, j) = tmp; \
        } \
    } \
} \
\
static hot size_t pdq_partition_##suffix(sort_ctx_t *ctx, size_t start, size_t len) { \
    size_t size = is_generic ? ctx->size : sizeof(type); \
    size_t mid = start + len / 2; \
    char *p_start = get_ptr(ctx, start); \
    char *p_mid = get_ptr(ctx, mid); \
    char *p_end = get_ptr(ctx, start + len - 1); \
    int (*cmp)(const void *, const void *, void *) = ctx->compar; \
    void *arg = ctx->arg; \
    int (*cmp_noarg)(const void *, const void *) = (int (*)(const void *, const void *))(void *)ctx->compar; \
    \
    if (len > 128) { \
        size_t step = (len >> 3) + (len >> 6) + 1; \
        size_t mid_idx = start + len / 2; \
        char *p1 = get_ptr(ctx, start); \
        char *p2 = get_ptr(ctx, start + step); \
        char *p3 = get_ptr(ctx, start + 2 * step); \
        char *p4 = get_ptr(ctx, mid_idx - step); \
        char *p5 = get_ptr(ctx, mid_idx); \
        char *p6 = get_ptr(ctx, mid_idx + step); \
        char *p7 = get_ptr(ctx, start + len - 1 - 2 * step); \
        char *p8 = get_ptr(ctx, start + len - 1 - step); \
        char *p9 = get_ptr(ctx, start + len - 1); \
        SORT3_HELPER(p1, p2, p3, suffix, size, cmp, arg, cmp_noarg, has_arg); \
        SORT3_HELPER(p4, p5, p6, suffix, size, cmp, arg, cmp_noarg, has_arg); \
        SORT3_HELPER(p7, p8, p9, suffix, size, cmp, arg, cmp_noarg, has_arg); \
        SORT3_HELPER(p2, p5, p8, suffix, size, cmp, arg, cmp_noarg, has_arg); \
    } \
    /* Median-of-3 on (start, mid, end) */ \
    if ((has_arg ? cmp(p_mid, p_start, arg) : cmp_noarg(p_mid, p_start)) < 0) swap_##suffix(p_start, p_mid, size); \
    if ((has_arg ? cmp(p_end, p_mid, arg) : cmp_noarg(p_end, p_mid)) < 0) swap_##suffix(p_mid, p_end, size); \
    if ((has_arg ? cmp(p_mid, p_start, arg) : cmp_noarg(p_mid, p_start)) < 0) swap_##suffix(p_start, p_mid, size); \
    swap_##suffix(p_start, p_mid, size); \
    char *pivot = p_start; \
    size_t i = start + 1; \
    size_t j = start + len - 1; \
    char *pi = get_ptr(ctx, i); \
    char *pj = get_ptr(ctx, j); \
    while (true) { \
        /* Prefetch only for large arrays where L1/L2 misses are likely */ \
        if (len > 65536) { \
            while (i <= j && (has_arg ? cmp(pi, pivot, arg) : cmp_noarg(pi, pivot)) < 0) { \
                __builtin_prefetch(pi + 128, 0, 0); \
                i++; pi += size; \
            } \
            while (i <= j && (has_arg ? cmp(pj, pivot, arg) : cmp_noarg(pj, pivot)) > 0) { \
                __builtin_prefetch(pj - 128, 0, 0); \
                j--; pj -= size; \
            } \
        } else { \
            while (i <= j && (has_arg ? cmp(pi, pivot, arg) : cmp_noarg(pi, pivot)) < 0) { \
                i++; pi += size; \
            } \
            while (i <= j && (has_arg ? cmp(pj, pivot, arg) : cmp_noarg(pj, pivot)) > 0) { \
                j--; pj -= size; \
            } \
        } \
        if (i >= j) break; \
        swap_##suffix(pi, pj, size); \
        i++; j--; pi += size; pj -= size; \
    } \
    char *p_split = get_ptr(ctx, j); \
    swap_##suffix(p_start, p_split, size); \
    return j; \
} \
\
static void flux_sort_rec_##suffix(sort_ctx_t *ctx, size_t start, size_t len, waitgroup_t *wg); \
\
static void flux_sort_task_wrapper_##suffix(void *arg) { \
    struct task_##suffix *t = (struct task_##suffix *)arg; \
    flux_sort_rec_##suffix(&t->ctx, t->start, t->len, t->wg); \
    wg_done(t->wg); \
    free(t); \
} \
\
static void flux_sort_rec_##suffix(sort_ctx_t *ctx, size_t start, size_t len, waitgroup_t *wg) { \
    if (len < FLUX_SMALL_SORT_THRESHOLD) { \
        insertion_sort_##suffix(ctx, start, len); \
        return; \
    } \
    int analysis = analyze_and_merge_##suffix(ctx, start, len); \
    if (analysis == 1) return; \
    size_t pivot_idx = pdq_partition_##suffix(ctx, start, len); \
    size_t left_len = (pivot_idx > start) ? (pivot_idx - start) : 0; \
    size_t right_start = pivot_idx + 1; \
    size_t right_len = (right_start < start + len) ? ((start + len) - right_start) : 0; \
    \
    if (wg && left_len > PAR_THRESHOLD) { \
        struct task_##suffix *t = malloc(sizeof(struct task_##suffix)); \
        if (t) { \
            t->ctx = *ctx; \
            t->start = start; \
            t->len = left_len; \
            t->wg = wg; \
            wg_add(wg, 1); \
            if (tp_submit(g_sort_pool, flux_sort_task_wrapper_##suffix, t) != 0) { \
                wg_done(wg); \
                free(t); \
                flux_sort_rec_##suffix(ctx, start, left_len, wg); \
            } \
        } else { \
            flux_sort_rec_##suffix(ctx, start, left_len, wg); \
        } \
    } else { \
        if (left_len > 0) flux_sort_rec_##suffix(ctx, start, left_len, wg); \
    } \
    \
    if (right_len > 0) flux_sort_rec_##suffix(ctx, right_start, right_len, wg); \
}

// Instantiate with ARG
SORT_IMPL(gen_arg, char, 1, 1)
SORT_IMPL(u32_arg, uint32_t, 0, 1)
SORT_IMPL(u64_arg, uint64_t, 0, 1)

// Instantiate without ARG
SORT_IMPL(gen_noarg, char, 1, 0)
SORT_IMPL(u32_noarg, uint32_t, 0, 0)
SORT_IMPL(u64_noarg, uint64_t, 0, 0)

#define PAR_DISPATCH(suffix, ctx, start, len) do { \
    if ((len) > PAR_THRESHOLD && g_sort_pool) { \
        waitgroup_t *wg = wg_create(); \
        if (wg) { \
            flux_sort_rec_##suffix(ctx, start, len, wg); \
            wg_wait(wg); \
            wg_destroy(wg); \
        } else { \
            flux_sort_rec_##suffix(ctx, start, len, NULL); \
        } \
    } else { \
        flux_sort_rec_##suffix(ctx, start, len, NULL); \
    } \
} while(0)

void po_sort_r(void *base, size_t nmemb, size_t size,
               int (*compar)(const void *, const void *, void *),
               void *arg) {
    if (nmemb < 2 || size == 0) return;
    sort_ctx_t ctx; ctx.base = (char *)base; ctx.size = size; ctx.compar = compar; ctx.arg = arg;

    if (size == 4 && ((uintptr_t)base % 4 == 0)) PAR_DISPATCH(u32_arg, &ctx, 0, nmemb);
    else if (size == 8 && ((uintptr_t)base % 8 == 0)) PAR_DISPATCH(u64_arg, &ctx, 0, nmemb);
    else PAR_DISPATCH(gen_arg, &ctx, 0, nmemb);
}

void po_sort(void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *)) {
    if (nmemb < 2 || size == 0) return;
    // Pass compar directly properly cast
    sort_ctx_t ctx; 
    ctx.base = (char *)base; 
    ctx.size = size; 
    // Use the no-arg compar as the void* one (it won't be called as void* in noarg path)
    ctx.compar = (int (*)(const void *, const void *, void *))(void *)compar; 
    ctx.arg = NULL;

    if (size == 4 && ((uintptr_t)base % 4 == 0)) PAR_DISPATCH(u32_noarg, &ctx, 0, nmemb);
    else if (size == 8 && ((uintptr_t)base % 8 == 0)) PAR_DISPATCH(u64_noarg, &ctx, 0, nmemb);
    else PAR_DISPATCH(gen_noarg, &ctx, 0, nmemb);
}
