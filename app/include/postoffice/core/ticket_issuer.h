/**
 * @file ticket_issuer.h
 * @ingroup executables
 * @brief Ticket Issuer – central allocator of sequential or sharded ticket IDs.
 *
 * Responsibilities
 * ----------------
 *  - Generates unique ticket identifiers (monotonic counters or composite keys).
 *  - Enforces rate limits or quota policies when configured.
 *  - Publishes tickets to workers or a shared queue.
 *
 * Consistency
 * -----------
 * Ensures no reuse / wrap-around within simulation bounds; may persist last issued id if
 * durability is required (future extension).
 */

#ifndef PO_CORE_TICKET_ISSUER_H
#define PO_CORE_TICKET_ISSUER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PO_CORE_TICKET_ISSUER_H */
