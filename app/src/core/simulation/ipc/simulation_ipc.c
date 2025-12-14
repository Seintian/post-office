#define _POSIX_C_SOURCE 200809L

#include "simulation_ipc.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* --- Private Helpers --- */

static sim_shm_t* _map_shm(int fd) {
    void* addr = mmap(NULL, sizeof(sim_shm_t), 
                      PROT_READ | PROT_WRITE, 
                      MAP_SHARED, fd, 0);
    
    if (addr == MAP_FAILED) {
        return NULL;
    }
    return (sim_shm_t*)addr;
}

/* --- Public API --- */

sim_shm_t* sim_ipc_shm_create(void) {
    // 1. shm_open with O_CREAT | O_EXCL to ensure we are the creator
    int fd = shm_open(SIM_SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0660);
    if (fd == -1) {
        // If EEXIST, maybe previous run crashed? 
        // For robustness, we could try to unlink and retry, 
        // but typically manual cleanup or a dedicated cleanup step is safer.
        return NULL;
    }

    // 2. ftruncate to set size
    if (ftruncate(fd, sizeof(sim_shm_t)) == -1) {
        close(fd);
        shm_unlink(SIM_SHM_NAME);
        return NULL;
    }

    // 3. Map
    sim_shm_t* shm = _map_shm(fd);
    close(fd); // valid to close fd after mmap

    if (shm) {
        // 4. Initialize (Zero out)
        memset(shm, 0, sizeof(sim_shm_t));
        
        // Initialize atomics explicitly if needed (though 0 is usually valid)
        atomic_init(&shm->time_control.sim_active, true);
        atomic_init(&shm->time_control.day, 1);
        atomic_init(&shm->time_control.hour, 8); // Default start 08:00
        atomic_init(&shm->time_control.minute, 0);
        atomic_init(&shm->time_control.elapsed_nanos, 0);
    } else {
        shm_unlink(SIM_SHM_NAME);
    }

    return shm;
}

sim_shm_t* sim_ipc_shm_attach(void) {
    // 1. shm_open existing
    int fd = shm_open(SIM_SHM_NAME, O_RDWR, 0660);
    if (fd == -1) {
        return NULL;
    }

    // 2. Map
    sim_shm_t* shm = _map_shm(fd);
    close(fd);

    return shm;
}

int sim_ipc_shm_detach(sim_shm_t* shm) {
    if (!shm) return -1;
    return munmap(shm, sizeof(sim_shm_t));
}


int sim_ipc_shm_destroy(void) {
    return shm_unlink(SIM_SHM_NAME);
}

#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>

/* --- Semaphore Helpers --- */

// Union generic for semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int sim_ipc_sem_create(int n_sems) {
    // Create semaphore set
    int semid = semget(SIM_SEM_KEY, n_sems, IPC_CREAT | IPC_EXCL | 0660);
    if (semid == -1) {
        if (errno == EEXIST) {
            // Try to grab existing
            semid = semget(SIM_SEM_KEY, n_sems, 0660);
        } else {
            return -1;
        }
    }

    // Initialize all to 0
    // In a real app we might want specific init values, but 0 is safe default
    // The caller (Director) should initialize specific sems if needed.
    // For concurrency safety, if we just created it, we should init.
    // Given IPC_EXCL, if we just created it:
    if (semid != -1) {
        unsigned short *values = calloc((size_t)n_sems, sizeof(unsigned short));
        if (values) {
            union semun arg;
            arg.array = values;
            semctl(semid, 0, SETALL, arg);
            free(values);
        }
    }
    return semid;
}

int sim_ipc_sem_get(void) {
    return semget(SIM_SEM_KEY, 0, 0660);
}

