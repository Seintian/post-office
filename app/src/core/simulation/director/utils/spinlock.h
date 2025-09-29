/** \file spinlock.h
 *  \ingroup director
 *  \brief Simple test-and-set spinlock for extremely short critical sections
 *         (protecting small shared statistics or free lists) where a mutex
 *         would impose undue syscall / futex overhead.
 *
 *  Semantics
 *  ---------
 *  - Unfair, busy waits until acquired.
 *  - Provides acquire barrier on lock, release barrier on unlock.
 *  - Optional pause/yield instruction in loop to reduce contention impact.
 *
 *  Usage Guidance
 *  --------------
 *  Keep critical sections tiny (< couple hundred cycles). For longer paths
 *  or potential blocking operations promote to a mutex.
 *
 *  Error Handling
 *  --------------
 *  Non-blocking trylock variant returns 0 on success, -1 otherwise (errno not
 *  set to keep hot path minimal). Recursive locking is undefined behavior.
 */
#ifndef PO_DIRECTOR_SPINLOCK_H
#define PO_DIRECTOR_SPINLOCK_H

/* Placeholder for spinlock struct & API (lock, unlock, trylock). */

#endif /* PO_DIRECTOR_SPINLOCK_H */
