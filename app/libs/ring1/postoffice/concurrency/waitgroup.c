#include "postoffice/concurrency/waitgroup.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

struct waitgroup_s {
    atomic_int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

waitgroup_t *wg_create(void) {
    waitgroup_t *wg = malloc(sizeof(waitgroup_t));
    if (!wg)
        return NULL;
    atomic_init(&wg->count, 0);
    pthread_mutex_init(&wg->mutex, NULL);
    pthread_cond_init(&wg->cond, NULL);
    return wg;
}

void wg_add(waitgroup_t *wg, int count) {
    atomic_fetch_add(&wg->count, count);
}

void wg_done(waitgroup_t *wg) {
    int prev = atomic_fetch_sub(&wg->count, 1);
    if (prev == 1) { // count dropped to 0
        pthread_mutex_lock(&wg->mutex);
        pthread_cond_broadcast(&wg->cond);
        pthread_mutex_unlock(&wg->mutex);
    }
}

void wg_wait(waitgroup_t *wg) {
    if (atomic_load(&wg->count) == 0)
        return;

    pthread_mutex_lock(&wg->mutex);
    while (atomic_load(&wg->count) > 0) {
        pthread_cond_wait(&wg->cond, &wg->mutex);
    }
    pthread_mutex_unlock(&wg->mutex);
}

int wg_active_count(waitgroup_t *wg) {
    if (!wg)
        return 0;
    return atomic_load(&wg->count);
}

void wg_destroy(waitgroup_t *wg) {
    pthread_mutex_destroy(&wg->mutex);
    pthread_cond_destroy(&wg->cond);
    free(wg);
}
