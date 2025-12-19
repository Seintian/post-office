#define _POSIX_C_SOURCE 200809L

#include "simulation_ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <postoffice/log/logger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* --- Private Helpers --- */

static key_t _get_ipc_key(void) {
    // Generate a user-specific lock file path for ftok
    char keypath[512];
    const char *user_home = getenv("HOME");

    if (user_home) {
        snprintf(keypath, sizeof(keypath), "%s/.postoffice/ipc.key", user_home);
        // Ensure dir exists (best effort without system())
        // Assuming director created ~/.postoffice, but we can try mkdir just in case
        // or just rely on fallback if open fails?
        // Let's assume directory likely exists or fallback to /tmp
    } else {
        snprintf(keypath, sizeof(keypath), "/tmp/postoffice_%d_ipc.key", getuid());
    }

    // Try to create/open the key file
    int fd = open(keypath, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        // Fallback to /tmp if home failed (e.g. dir missing)
        snprintf(keypath, sizeof(keypath), "/tmp/postoffice_%d_ipc.key", getuid());
        fd = open(keypath, O_CREAT | O_RDWR, 0666);
    }

    if (fd != -1) {
        close(fd);
    } else {
        LOG_ERROR("Failed to create IPC key file %s: %s", keypath, strerror(errno));
        return -1;
    }

    key_t key = ftok(keypath, 'P');
    if (key == -1) {
        LOG_ERROR("ftok failed on %s: %s", keypath, strerror(errno));
    }
    return key;
}

/* --- Public API --- */

sim_shm_t *sim_ipc_shm_create(size_t n_workers) {
    LOG_INFO("sim_ipc_shm_create() - Creating SHM: %s with %zu workers", SIM_SHM_NAME, n_workers);

    const int max_retries = 3;
    int retry_count = 0;
    int shm_fd = -1;
    void *ptr = MAP_FAILED;
    // Calculate total size including the flexible array member
    size_t total_size = sizeof(sim_shm_t) + (n_workers * sizeof(worker_status_t));

    while (retry_count < max_retries) {
        // 1. Unlink stale object to ensure fresh creation
        shm_unlink(SIM_SHM_NAME);

        // 2. Open with O_CREAT | O_EXCL to ensure we are the creator
        shm_fd = shm_open(SIM_SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0666);
        if (shm_fd == -1) {
            if (errno == EEXIST) {
                LOG_WARN("sim_ipc_shm_create() - SHM exists, retrying unlink... (%d/%d)",
                         retry_count + 1, max_retries);
                usleep(100000); // Wait 100ms
                retry_count++;
                continue;
            }
            LOG_ERROR("sim_ipc_shm_create() - shm_open failed: %s", strerror(errno));
            return NULL;
        }

        // 3. Set size
        if (ftruncate(shm_fd, (off_t)total_size) == -1) {
            LOG_ERROR("sim_ipc_shm_create() - ftruncate failed: %s", strerror(errno));
            close(shm_fd);
            shm_unlink(SIM_SHM_NAME);
            return NULL;
        }

        // 4. Map
        ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (ptr == MAP_FAILED) {
            LOG_ERROR("sim_ipc_shm_create() - mmap failed: %s", strerror(errno));
            close(shm_fd);
            shm_unlink(SIM_SHM_NAME);
            return NULL;
        }

        // 5. Zero out
        memset(ptr, 0, total_size);

        // 6. Initialize Parameters
        sim_shm_t *shm = (sim_shm_t *)ptr;
        shm->params.n_workers = (uint32_t)n_workers;

        // Initialize time
        atomic_init(&shm->time_control.sim_active, true);
        uint64_t initial_time =
            ((uint64_t)DEFAULT_START_DAY << 16) | ((uint64_t)DEFAULT_START_HOUR << 8) | 0ULL;
        atomic_init(&shm->time_control.packed_time, initial_time);
        atomic_init(&shm->time_control.elapsed_nanos, 0);

        // Initialize Synchronization Primitives (PTHREAD_PROCESS_SHARED)
        pthread_mutexattr_t mattr;
        pthread_mutexattr_init(&mattr);
        pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&shm->sync.mutex, &mattr);
        // Initialize Time Mutex
        pthread_mutex_init(&shm->time_control.mutex, &mattr);
        
        // Initialize Queue Mutexes
        for (int i = 0; i < SIM_MAX_SERVICE_TYPES; i++) {
            pthread_mutex_init(&shm->queues[i].mutex, &mattr);
        }
        pthread_mutexattr_destroy(&mattr);

        pthread_condattr_t cattr;
        pthread_condattr_init(&cattr);
        pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
        // Using CLOCK_MONOTONIC for robust timed waits if needed
        pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);

        pthread_cond_init(&shm->sync.cond_workers_ready, &cattr);
        pthread_cond_init(&shm->sync.cond_day_start, &cattr);
        // Initialize Time Tick Cond
        pthread_cond_init(&shm->time_control.cond_tick, &cattr);

        // Initialize Queue Condition Variables
        for (int i = 0; i < SIM_MAX_SERVICE_TYPES; i++) {
            pthread_cond_init(&shm->queues[i].cond_added, &cattr);
            pthread_cond_init(&shm->queues[i].cond_served, &cattr);
        }
        pthread_condattr_destroy(&cattr);

        close(shm_fd);
        LOG_INFO("sim_ipc_shm_create() - SHM Created at %p (size: %zu)", ptr, total_size);
        return shm;
    }

    LOG_ERROR("sim_ipc_shm_create() - Failed to create unique SHM object after retries");
    return NULL;
}

sim_shm_t *sim_ipc_shm_attach(void) {
    LOG_INFO("sim_ipc_shm_attach() - Attaching to SHM: %s", SIM_SHM_NAME);

    int shm_fd = shm_open(SIM_SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        LOG_ERROR("sim_ipc_shm_attach() - shm_open failed: %s", strerror(errno));
        return NULL;
    }

    // Determine size of existing object
    struct stat statbuf;
    if (fstat(shm_fd, &statbuf) == -1) {
        LOG_ERROR("sim_ipc_shm_attach() - fstat failed: %s", strerror(errno));
        close(shm_fd);
        return NULL;
    }

    size_t total_size = (size_t)statbuf.st_size;
    if (total_size < sizeof(sim_shm_t)) {
        LOG_ERROR("sim_ipc_shm_attach() - SHM too small: %zu < %zu", total_size, sizeof(sim_shm_t));
        close(shm_fd);
        return NULL;
    }

    void *ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd); // Close FD, map remains

    if (ptr == MAP_FAILED) {
        LOG_ERROR("sim_ipc_shm_attach() - mmap failed: %s", strerror(errno));
        return NULL;
    }

    sim_shm_t *shm = (sim_shm_t *)ptr;
    // Basic validation
    size_t expected = sizeof(sim_shm_t) + (shm->params.n_workers * sizeof(worker_status_t));
    if (total_size != expected) {
        LOG_WARN("sim_ipc_shm_attach() - Size mismatch warning: Mapped %zu, Expected %zu based on "
                 "n_workers=%u",
                 total_size, expected, shm->params.n_workers);
    }

    LOG_INFO("sim_ipc_shm_attach() - Attached at %p (n_workers=%u)", ptr, shm->params.n_workers);
    return shm;
}

/**
 * @brief Detaches the shared memory.
 * @param shm Pointer to SHM.
 * @return 0 on success, -1 on failure.
 */
int sim_ipc_shm_detach(sim_shm_t *shm) {
    if (!shm)
        return -1;

    /*
       To unmap correctly we should know the original size.
       However, munmap expects the length.
       Since we don't store the length in the process (unless we cache it),
       we can re-calculate it from shm->params.n_workers because we still have access to the mapped
       memory at this point.
    */
    size_t size = sizeof(sim_shm_t) + (shm->params.n_workers * sizeof(worker_status_t));

    if (munmap(shm, size) == -1) {
        LOG_ERROR("sim_ipc_shm_detach() - munmap failed: %s", strerror(errno));
        return -1;
    }
    LOG_INFO("sim_ipc_shm_detach() - Detached SHM");
    return 0;
}

/**
 * @brief Destroys the shared memory object.
 * @return 0 on success, -1 on failure (errno set).
 */
int sim_ipc_shm_destroy(void) {
    return shm_unlink(SIM_SHM_NAME);
}

/* --- Semaphore Helpers --- */

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/**
 * @brief Creates or gets a System V semaphore set.
 * @param n_sems Number of semaphores. Must be > 0.
 * @return Semaphore ID on success, -1 on failure.
 */
int sim_ipc_sem_create(int n_sems) {
    if (n_sems <= 0) {
        errno = EINVAL;
        LOG_ERROR("Invalid n_sems: %d", n_sems);
        return -1;
    }

    key_t key = _get_ipc_key();
    if (key == -1)
        return -1;

    int semid = -1;

    // Try to create exclusively first
    semid = semget(key, n_sems, IPC_CREAT | IPC_EXCL | 0660);

    if (semid != -1) {
        // Initialize
        unsigned short *values = calloc((size_t)n_sems, sizeof(unsigned short));
        if (!values) {
            semctl(semid, 0, IPC_RMID);
            LOG_ERROR("calloc failed during sem init");
            return -1;
        }

        union semun arg;
        arg.array = values;
        if (semctl(semid, 0, SETALL, arg) == -1) {
            LOG_ERROR("semctl SETALL failed: %s", strerror(errno));
            free(values);
            semctl(semid, 0, IPC_RMID);
            return -1;
        }
        free(values);
        return semid;
    }

    if (errno == EEXIST) {
        // Already exists - connect and RESET
        semid = semget(key, n_sems, 0660);
        if (semid != -1) {
            // Reset to 0 to ensure clean state for new run
            unsigned short *values = calloc((size_t)n_sems, sizeof(unsigned short));
            if (values) {
                union semun arg;
                arg.array = values;
                if (semctl(semid, 0, SETALL, arg) == -1) {
                    LOG_WARN("semctl RESET failed: %s", strerror(errno));
                }
                free(values);
            }
        }
        return semid;
    }

    LOG_ERROR("semget create failed: %s", strerror(errno));
    return -1;
}

int sim_ipc_sem_get(void) {
    key_t key = _get_ipc_key();
    if (key == -1)
        return -1;
    int semid = semget(key, 0, 0660);
    if (semid == -1) {
        // Silent fail on ENOENT is okay for probers, but explicit failure otherwise
        if (errno != ENOENT) {
            LOG_ERROR("semget get failed: %s", strerror(errno));
        }
    }
    return semid;
}
