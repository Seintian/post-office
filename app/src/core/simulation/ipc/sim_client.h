#ifndef SIMULATION_CLIENT_H
#define SIMULATION_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <signal.h>
#include "simulation_ipc.h"

// --- Connection ---
/**
 * @brief Retry loop to connect to the Ticket Issuer via UNIX socket.
 * 
 * @param should_continue Pointer to atomic flag for interruption.
 * @param shm Pointer to SHM for logging time context (optional).
 * @return Socket FD on success, -1 on failure/shutdown.
 */
int sim_client_connect_issuer(volatile _Atomic bool *should_continue, sim_shm_t *shm);

// --- Synchronization ---
/**
 * @brief Reads current simulation time from SHM.
 */
void sim_client_read_time(sim_shm_t *shm, int *day, int *hour, int *minute);

/**
 * @brief Waits for the daily synchronization barrier.
 * 
 * @param shm Shared memory pointer.
 * @param last_synced_day Pointer to local day tracker.
 * @param g_running Pointer to global running flag.
 */
void sim_client_wait_barrier(sim_shm_t *shm, int *last_synced_day, volatile sig_atomic_t *g_running);

// --- Signal Handling ---
/**
 * @brief Sets up standard termination signals (SIGINT, SIGTERM).
 * @param handler Function pointer to signal handler.
 */
void sim_client_setup_signals(void (*handler)(int, siginfo_t*, void*));

#endif
