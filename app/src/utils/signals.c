/**
 * @file signals.c
 * @brief Utility functions for handling signals.
 *
 * @see signals.h
 */

#ifndef _GNU_SOURCE
// Required by IDE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>

#include "signals.h"

/*
 * Use compile-time macros for terminating signals and their count.
 * `COMMON_TERMINATING_SIGNALS` is expected to expand to a comma-separated
 * list of integers (signal constants). We create a compound literal
 * expression for the array and compute its size via sizeof so that
 * callers can use `TERMINATING_SIGNALS` and `NUM_TERMINATING_SIGNALS`.
 */
#define TERMINATING_SIGNALS ((const int[]){ COMMON_TERMINATING_SIGNALS })
#define NUM_TERMINATING_SIGNALS (sizeof(TERMINATING_SIGNALS) / sizeof((TERMINATING_SIGNALS)[0]))

/**
 * @brief Set the signals to sigset object
 *
 * @param[in] signals array of `num_signals` signals
 * @param[in] num_signals number of signals in the array
 * @param[out] sigset pointer to the sigset object to modify
 * @return 0 on success, -1 on error
 */
static int set_signals_to_sigset(const int* signals, size_t num_signals, sigset_t* sigset) {
    if (sigemptyset(sigset) == -1) {
        return -1;
    }

    for (size_t i = 0; i < num_signals; i++) {
        if (sigaddset(sigset, signals[i]) == -1) {
            return -1;
        }
    }

    return 0;
}

/* API */

int sigutil_setup(signals_handler_t handler, unsigned char flag, int handler_flags) {
    if (sigutil_restore_all() == -1)
        return -1;

    if ((flag & (SIGUTIL_HANDLE_ALL_SIGNALS | SIGUTIL_HANDLE_TERMINATING_ONLY | SIGUTIL_HANDLE_NON_TERMINATING)) && !handler) {
        errno = EINVAL;
        return -1;
    }


    if ((flag & SIGUTIL_HANDLE_ALL_SIGNALS) == SIGUTIL_HANDLE_ALL_SIGNALS) {
        if (sigutil_handle_all(handler, handler_flags) == -1)
            return -1;
    } else {
        if ((flag & SIGUTIL_HANDLE_TERMINATING_ONLY) && sigutil_handle_terminating(handler, handler_flags) == -1)
            return -1;

        if ((flag & SIGUTIL_HANDLE_NON_TERMINATING) && sigutil_handle_non_terminating(handler, handler_flags) == -1)
            return -1;
    }

    if ((flag & SIGUTIL_BLOCK_ALL_SIGNALS) == SIGUTIL_BLOCK_ALL_SIGNALS) {
        if (sigutil_block_all() == -1)
            return -1;
    } else {
        if ((flag & SIGUTIL_BLOCK_TERMINATING_ONLY) && sigutil_block_terminating() == -1)
            return -1;

        if ((flag & SIGUTIL_BLOCK_NON_TERMINATING) && sigutil_block_non_terminating() == -1)
            return -1;
    }

    return 0;
}

int sigutil_handle(int signum, signals_handler_t handler, int flags) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO | flags;
    sa.sa_sigaction = handler;

    if (sigemptyset(&sa.sa_mask) == -1)
        return -1;

    if (sigaction(signum, &sa, NULL) == -1)
        return -1;

    return 0;
}

int sigutil_handle_all(signals_handler_t handler, int flags) {
    for (int i = 1; i < NSIG; i++) {
        if (sigutil_handle(i, handler, flags) == -1) {
            if (errno == EINVAL)
                continue;

            return -1;
        }
    }

    return 0;
}

int sigutil_handle_terminating(signals_handler_t handler, int flags) {
    for (size_t i = 0; i < NUM_TERMINATING_SIGNALS; i++) {
        int sig = TERMINATING_SIGNALS[i];
        if (sigutil_handle(sig, handler, flags) == -1) {
            if (errno == EINVAL)
                continue;
            return -1;
        }
    }

    return 0;
}
int sigutil_handle_non_terminating(signals_handler_t handler, int flags) {
    for (int i = 1; i < NSIG; i++) {
        bool is_terminating = false;

        for (size_t j = 0; j < NUM_TERMINATING_SIGNALS; j++) {
            if (i == TERMINATING_SIGNALS[j]) {
                is_terminating = true;
                break;
            }
        }

        if (!is_terminating && sigutil_handle(i, handler, flags) == -1) {
            if (errno == EINVAL)
                continue;
            return -1;
        }
    }

    return 0;
}

int sigutil_block(int signum) {
    sigset_t sigset;
    if (sigemptyset(&sigset) == -1)
        return -1;

    if (sigaddset(&sigset, signum) == -1)
        return -1;

    if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
        return -1;

    return 0;
}

int sigutil_block_all(void) {
    sigset_t sigset;
    if (sigfillset(&sigset) == -1)
        return -1;

    if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
        return -1;

    return 0;
}

int sigutil_block_terminating(void) {
    sigset_t sigset;
    if (set_signals_to_sigset(TERMINATING_SIGNALS, NUM_TERMINATING_SIGNALS, &sigset) == -1)
        return -1;

    if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
        return -1;

    return 0;
}

int sigutil_block_non_terminating(void) {
    int* non_terminating_signals = malloc(sizeof(int) * (NSIG - NUM_TERMINATING_SIGNALS - 1));
    if (!non_terminating_signals)
        return -1;

    size_t j = 0;
    for (int i = 1; i < NSIG; i++) {
        bool is_non_terminating = true;

        for (size_t k = 0; k < NUM_TERMINATING_SIGNALS; k++) {
            if (i == TERMINATING_SIGNALS[k]) {
                is_non_terminating = false;
                break;
            }
        }

        if (is_non_terminating) {
            non_terminating_signals[j] = i;
            j++;
        }
    }

    sigset_t sigset;
    int ret = set_signals_to_sigset(non_terminating_signals, j, &sigset);

    if (ret == 0 && sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
        ret = -1;

    free(non_terminating_signals);
    return ret;
}

int sigutil_unblock(int signum) {
    sigset_t sigset;
    if (sigemptyset(&sigset) == -1)
        return -1;

    if (sigaddset(&sigset, signum) == -1)
        return -1;

    if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) == -1)
        return -1;

    return 0;
}

int sigutil_unblock_all(void) {
    sigset_t sigset;
    if (sigfillset(&sigset) == -1)
        return -1;

    if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) == -1)
        return -1;

    return 0;
}

int sigutil_get_blocked_signals(sigset_t* set) {
    if (sigprocmask(SIG_BLOCK, NULL, set) == -1)
        return -1;

    return 0;
}

int sigutil_restore_all(void) {
    if (sigutil_unblock_all() == -1)
        return -1;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;

    for (int i = 1; i < NSIG; i++) {
        if (sigaction(i, &sa, NULL) == -1) {
            if (errno == EINVAL)
                continue;  /* ignore non-catchable signals */
            return -1;
        }
    }

    return 0;
}

int sigutil_restore(int signum) {
    if (sigutil_unblock(signum) == -1)
        return -1;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;

    if (sigaction(signum, &sa, NULL) == -1)
        return -1;

    return 0;
}

int sigutil_wait(int signum) {
    if (sigutil_block(signum) == -1)
        return -1;

    sigset_t sigset;
    if (sigemptyset(&sigset) == -1)
        return -1;

    if (sigaddset(&sigset, signum) == -1)
        return -1;

    int sig;
    int ret = sigwait(&sigset, &sig);

    if (sigutil_unblock(signum) == -1)
        return -1;

    return ret;
}

int sigutil_wait_any(void) {
    sigset_t old_sigset;
    if (sigprocmask(SIG_BLOCK, NULL, &old_sigset) == -1)
        return -1;

    sigset_t new_sigset;
    if (sigfillset(&new_sigset) == -1)
        return -1;

    int sig;
    if (sigwait(&new_sigset, &sig) == -1)
        return -1;

    /* Restore the old sigset of blocked signals */
    if (sigprocmask(SIG_SETMASK, &old_sigset, NULL) == -1)
        return -1;

    /* Raise the signal to ensure terminating ones are not ignored */
    if (raise(sig) == -1)
        return -1;

    return sig;
}

int sigutil_signal_children(int sig) {
    /* Block all signals to ensure atomicity of the PGID swap */
    sigset_t old_mask, all_mask;
    if (sigfillset(&all_mask) == -1) return -1;
    if (sigprocmask(SIG_BLOCK, &all_mask, &old_mask) == -1) return -1;

    /* Save the current process group ID */
    pid_t old_pgid = getpgid(0);
    if (old_pgid == -1) {
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return -1;
    }

    /* Get the parent Process Group ID */
    pid_t new_pgid = getpgid(getppid());
    if (new_pgid == -1) {
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return -1;
    }

    /* Switch process group */
    if (setpgid(0, new_pgid) == -1) {
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return -1;
    }

    /* Signal the group (which should include siblings now, but we are signaling old_pgid?)
       Wait, the original logic was:
       setpgid(0, parent_pgid); 
       killpg(old_pgid, sig);  <-- Signal the GROUP we just left?
       setpgid(0, old_pgid);

       If we change PGID to parent's, we are no longer in `old_pgid`. 
       If `old_pgid` was our own private group (common for leaders), then `killpg(old_pgid)` signals our children.
       This logic seems intended to signal "my children" by:
       1. Leaving the group (so I don't signal myself, if I was the leader?)
       2. Signaling the group.
       3. Rejoining.
       
       Yes, that seems to be the intent.
    */
    int ret = killpg(old_pgid, sig);

    /* Restore process group */
    if (setpgid(0, old_pgid) == -1) {
        /* This is bad: we are stuck in parent's group. 
           But we can't do much else than report error. */
        ret = -1; 
    }

    /* Restore signal mask */
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    return ret == 0 ? 0 : -1;
}
