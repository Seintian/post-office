#define _POSIX_C_SOURCE 200809L
#include "director_time.h"
#include <postoffice/log/logger.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

void synchronize_simulation_barrier(sim_shm_t *shm, int day, volatile sig_atomic_t *running_flag) {
    if (!*running_flag) return;

    pthread_mutex_lock(&shm->sync.mutex);

    atomic_store(&shm->sync.ready_count, 0);
    atomic_store(&shm->sync.day_seq, (unsigned int)day);
    atomic_thread_fence(memory_order_release);
    atomic_store(&shm->sync.barrier_active, 1);

    // Wake workers
    for (int i=0; i<SIM_MAX_SERVICE_TYPES; i++) {
        pthread_mutex_lock(&shm->queues[i].mutex);
        pthread_cond_broadcast(&shm->queues[i].cond_added);
        pthread_mutex_unlock(&shm->queues[i].mutex);
    }

    uint32_t req = atomic_load(&shm->sync.required_count);
    LOG_DEBUG("Synchronizing Day %d (Waiting for %u participants)...", day, req);

    struct timespec ts;
    while (*running_flag) {
        if (atomic_load(&shm->sync.ready_count) >= req) break;

        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_nsec += 100000000; 
        if(ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }

        pthread_cond_timedwait(&shm->sync.cond_workers_ready, &shm->sync.mutex, &ts);
    }

    atomic_store(&shm->sync.barrier_active, 0);
    pthread_cond_broadcast(&shm->sync.cond_day_start);
    pthread_mutex_unlock(&shm->sync.mutex);

    LOG_DEBUG("Day %d Synchronized.", day);
}

void execute_simulation_clock_loop(sim_shm_t *shm, volatile sig_atomic_t *running_flag, int expected_users) {
    if (expected_users > 0) {
        LOG_INFO("Waiting for %d users to connect...", expected_users);
        while (*running_flag) {
            unsigned int current = atomic_load(&shm->stats.connected_users);
            if (current >= (unsigned int)expected_users) break;
            usleep(10000); // 10ms poll
        }
        LOG_INFO("All expected users connected (%u/%d). Starting.", 
        atomic_load(&shm->stats.connected_users), expected_users);
    }

    int day = 1;
    int hour = 0; 
    int minute = 0;

    atomic_store(&shm->time_control.sim_active, true);

    synchronize_simulation_barrier(shm, day, running_flag);
    LOG_INFO("Simulation Clock Started.");

    while(*running_flag) {
        // Update SHM
        pthread_mutex_lock(&shm->time_control.mutex);
        uint64_t packed = ((uint64_t)day << 16) | ((uint64_t)hour << 8) | (uint64_t)minute;
        atomic_store(&shm->time_control.packed_time, packed);
        pthread_cond_broadcast(&shm->time_control.cond_tick);
        pthread_mutex_unlock(&shm->time_control.mutex);

        LOG_TRACE("Tick: Day %d %02d:%02d", day, hour, minute);

        // Wait
        uint64_t sleep = shm->params.tick_nanos / 1000;
        if (sleep > 0) usleep((useconds_t)sleep);

        // Check for Opening Time (08:00)
        if (hour == 8 && minute == 0) {
            LOG_INFO("Office Opening (08:00)");
        }

        // Check for Closing Time (17:00)
        if (hour == 17 && minute == 0) {
            LOG_INFO("Office Closing (17:00) - Interrupting all active work/queues.");
            for (int i=0; i<SIM_MAX_SERVICE_TYPES; i++) {
                // Determine how many waiting
                unsigned int waiting = atomic_load(&shm->queues[i].waiting_count);
                if (waiting > 0) {
                    LOG_DEBUG("Flushing Queue %d (%u users waiting)", i, waiting);
                    // Wake all users waiting on cond_served -> they will check time >= 17 and leave
                    pthread_mutex_lock(&shm->queues[i].mutex);
                    pthread_cond_broadcast(&shm->queues[i].cond_served);
                    pthread_mutex_unlock(&shm->queues[i].mutex);
                }
            }
        }

        // Advance
        minute++;
        if (minute >= 60) {
            minute = 0; hour++;
            if(hour >= 24) {
                hour=0; day++;
                if (shm->params.sim_duration_days > 0 && (uint32_t)day > shm->params.sim_duration_days) {
                    LOG_INFO("Duration %d days reached.", shm->params.sim_duration_days);
                    break;
                }
                synchronize_simulation_barrier(shm, day, running_flag);
            }
        }

        // Explode Check
        if (shm->params.explode_threshold > 0) {
            uint32_t total=0;
            for(int i=0; i<SIM_MAX_SERVICE_TYPES; i++) 
                total += atomic_load(&shm->queues[i].waiting_count);
            if(total > shm->params.explode_threshold) {
                LOG_FATAL("MELTDOWN: Queue Overflow.");
                break;
            }
        }
    }
    atomic_store(&shm->time_control.sim_active, false);
}
