/**
 * @file shared.h
 * @ingroup executables
 * @brief Shared types, enums, and constants exchanged between simulation processes.
 *
 * Typical Contents (implementation-specific)
 * ------------------------------------------
 *  - IPC message enums / opcodes
 *  - Common struct layouts (versioned if extended)
 *  - Limits (max users, max tickets, buffer sizes)
 *
 * Modification Guidelines
 * -----------------------
 * Changes to shared wire structs should be backward compatible within a run; consider version
 * tagging or size fields when adding optional data.
 */

#ifndef PO_CORE_SHARED_H
#define PO_CORE_SHARED_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PO_CORE_SHARED_H */
