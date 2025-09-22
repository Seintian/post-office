/**
 * @file macros.h
 * @ingroup tui_internal
 * @brief Common macros and utilities for internal use.
 */

#ifndef POSTOFFICE_TUI_INTERNAL_MACROS_H
#define POSTOFFICE_TUI_INTERNAL_MACROS_H

#include <assert.h>

#define PO_TUI_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define PO_TUI_MIN(a, b) ((a) < (b) ? (a) : (b))
#define PO_TUI_MAX(a, b) ((a) > (b) ? (a) : (b))

#if defined(__GNUC__) || defined(__clang__)
#define PO_TUI_LIKELY(x) __builtin_expect(!!(x), 1)
#define PO_TUI_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define PO_TUI_LIKELY(x) (x)
#define PO_TUI_UNLIKELY(x) (x)
#endif

#define PO_TUI_UNUSED(x) (void)(x)

#endif /* POSTOFFICE_TUI_INTERNAL_MACROS_H */
