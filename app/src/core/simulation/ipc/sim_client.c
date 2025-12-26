/**
 * @file sim_client.c
 * @brief Shared logic for simulation clients (User, Worker, etc).
 */

#define _POSIX_C_SOURCE 200809L

#include "sim_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>

#include <postoffice/log/logger.h>
#include <postoffice/net/socket.h>
#include <utils/signals.h>

// --- Connection ---

int sim_client_connect_issuer(volatile _Atomic bool *should_continue, sim_shm_t *shm) {
    const char *user_home = getenv("HOME");
    char sock_path[512];
    if (user_home) {
        snprintf(sock_path, sizeof(sock_path), "%s/.postoffice/issuer.sock", user_home);
    } else {
        snprintf(sock_path, sizeof(sock_path), "/tmp/postoffice_%d_issuer.sock", getuid());
    }

    LOG_DEBUG("Attempting to connect to Ticket Issuer socket: %s", sock_path); // Keep DEBUG or TRACE

    int socket_fd = -1;
    // Retry for ~10 seconds (500 * 20ms) to accommodate slow starts (e.g. ASan)
    for (int i = 0; i < 500; i++) {
        if (should_continue && !atomic_load(should_continue)) break;

        socket_fd = po_socket_connect_unix(sock_path);
        if (socket_fd >= 0) {
            po_socket_set_blocking(socket_fd);
            LOG_INFO("Successfully connected to Ticket Issuer (fd=%d)", socket_fd);
            break;
        }

        // Only log retry warnings every 10 attempts to avoid spam
        if (i > 0 && i % 10 == 0) {
            LOG_WARN("Retrying Ticket Issuer connection... (%d/100)", i);
        }
        usleep(20000); // 20ms
    }

    if (socket_fd < 0 && (!should_continue || atomic_load(should_continue))) {
        int d = 0, h = 0, m = 0;
        if (shm) sim_client_read_time(shm, &d, &h, &m);
        LOG_ERROR("[Day %d %02d:%02d] Failed to connect to %s after retries (errno=%d: %s)", 
            d, h, m, sock_path, errno, strerror(errno));
    }

    return socket_fd;
}

// --- Time & Sync ---

void sim_client_read_time(sim_shm_t *shm, int *day, int *hour, int *minute) {
    if (!shm) return;
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *day = (packed >> 16) & 0xFFFF;
    *hour = (packed >> 8) & 0xFF;
    *minute = packed & 0xFF;
}

void sim_client_wait_barrier(sim_shm_t *shm, int *last_synced_day, volatile sig_atomic_t *g_running) {
    if (!shm || !g_running) return;

    while (!*g_running && atomic_load(&shm->sync.barrier_active)) {
        unsigned int barrier_day = atomic_load(&shm->sync.day_seq);
        if ((int)barrier_day > *last_synced_day) {
            // Signal readiness
            atomic_fetch_add(&shm->sync.ready_count, 1);
            *last_synced_day = (int)barrier_day;

            // Notify Director we are ready
            pthread_mutex_lock(&shm->sync.mutex);
            pthread_cond_signal(&shm->sync.cond_workers_ready);

            // Wait for release
            struct timespec ts;
            while (!*g_running && atomic_load(&shm->sync.barrier_active) &&
                atomic_load(&shm->sync.day_seq) == barrier_day) {

                clock_gettime(CLOCK_MONOTONIC, &ts);
                ts.tv_sec += 1; 
                pthread_cond_timedwait(&shm->sync.cond_day_start, &shm->sync.mutex, &ts);
            }
            pthread_mutex_unlock(&shm->sync.mutex);
            break; // Finished this barrier
        } else {
            // Barrier active but we already joined or it's old day
            pthread_mutex_lock(&shm->sync.mutex);
            if (!*g_running && atomic_load(&shm->sync.barrier_active)) {
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                ts.tv_nsec += 50000000; // 50ms
                if(ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
                pthread_cond_timedwait(&shm->sync.cond_day_start, &shm->sync.mutex, &ts);
            }
            pthread_mutex_unlock(&shm->sync.mutex);
            if (!atomic_load(&shm->sync.barrier_active)) break; 
        }
    }
}

// --- Signals ---

void sim_client_setup_signals(void (*handler)(int, siginfo_t*, void*)) {
    struct sigaction sa;
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}
