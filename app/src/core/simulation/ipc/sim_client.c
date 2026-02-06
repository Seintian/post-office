/**
 * @file sim_client.c
 * @brief Shared logic for simulation clients (User, Worker, etc).
 */

#define _POSIX_C_SOURCE 200809L

#include "sim_client.h"

#include <errno.h>
#include <postoffice/log/logger.h>
#include <postoffice/net/socket.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <utils/signals.h>

// --- Connection ---

int sim_client_connect_issuer(volatile atomic_bool *should_continue, sim_shm_t *shm) {
    const char *user_home = getenv("HOME");
    char sock_path[512];
    if (user_home) {
        snprintf(sock_path, sizeof(sock_path), "%s/.postoffice/issuer.sock", user_home);
    } else {
        snprintf(sock_path, sizeof(sock_path), "/tmp/postoffice_%d_issuer.sock", getuid());
    }

    LOG_DEBUG("Attempting to connect to Ticket Issuer socket: %s",
              sock_path); // Keep DEBUG or TRACE

    int socket_fd = -1;
    // Retry for ~10 seconds (500 * 20ms) to accommodate slow starts (e.g. ASan)
    for (int i = 0; i < 500; i++) {
        if (should_continue && !atomic_load(should_continue))
            break;

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
        if (shm)
            sim_client_read_time(shm, &d, &h, &m);

        LOG_ERROR("[Day %d %02d:%02d] Failed to connect to %s after retries (errno=%d: %s)", d, h,
                  m, sock_path, errno, strerror(errno));
    }

    return socket_fd;
}

// --- Time & Sync ---

void sim_client_read_time(sim_shm_t *shm, int *day, int *hour, int *minute) {
    if (!shm)
        return;
    uint64_t packed = atomic_load(&shm->time_control.packed_time);
    *day = (packed >> 16) & 0xFFFF;
    *hour = (packed >> 8) & 0xFF;
    *minute = packed & 0xFF;
}

void sim_client_wait_barrier(sim_shm_t *shm, int *last_synced_day,
                             volatile sig_atomic_t *g_shutdown_flag) {
    if (!shm || !g_shutdown_flag)
        return;

    while (!*g_shutdown_flag) {
        unsigned int barrier_day = atomic_load(&shm->sync.day_seq);

        // Check if we need to sync for a new day
        if ((int)barrier_day > *last_synced_day) {
            int retries = 0;
            while (!*g_shutdown_flag && !atomic_load(&shm->sync.barrier_active)) {
                usleep(1000);           // 1ms wait
                if (++retries > 5000) { // 5s timeout safety
                    LOG_WARN("Waiting for barrier activation for Day %u...", barrier_day);
                    retries = 0;
                }
            }
            if (*g_shutdown_flag)
                return;

            // Join the Barrier
            pthread_mutex_lock(&shm->sync.mutex);

            // Verify active again under lock (though it's atomic)
            if (atomic_load(&shm->sync.barrier_active)) {
                atomic_fetch_add(&shm->sync.ready_count, 1);
                pthread_cond_signal(&shm->sync.cond_workers_ready);

                // Update local state
                *last_synced_day = (int)barrier_day;

                // Wait for Release
                struct timespec ts;
                while (!*g_shutdown_flag && atomic_load(&shm->sync.barrier_active)) {
                    clock_gettime(CLOCK_MONOTONIC, &ts);
                    ts.tv_sec += 1;
                    pthread_cond_timedwait(&shm->sync.cond_day_start, &shm->sync.mutex, &ts);
                }
            }
            pthread_mutex_unlock(&shm->sync.mutex);
            return; // Synced!
        }

        usleep(5000); // 5ms poll
    }
}

// --- Signals ---

void sim_client_setup_signals(void (*handler)(int, siginfo_t *, void *)) {
    if (sigutil_setup(handler, SIGUTIL_HANDLE_TERMINATING_ONLY, 0) != 0) {
        LOG_FATAL("Failed to setup signals");
        exit(1);
    }

    if (sigutil_handle(SIGPIPE, SIG_IGN, 0) != 0) {
        LOG_WARN("Failed to ignore SIGPIPE");
    }
}
