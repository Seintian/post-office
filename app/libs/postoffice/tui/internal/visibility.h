/**
 * @file visibility.h
 * @ingroup tui_internal
 * @brief Symbol visibility and inline attributes for the TUI library.
 */

#ifndef POSTOFFICE_TUI_INTERNAL_VISIBILITY_H
#define POSTOFFICE_TUI_INTERNAL_VISIBILITY_H

#if defined(__GNUC__) || defined(__clang__)
#define PO_TUI_API __attribute__((visibility("default")))
#define PO_TUI_LOCAL __attribute__((visibility("hidden")))
#else
#define PO_TUI_API
#define PO_TUI_LOCAL
#endif

#if !defined(PO_TUI_INLINE)
#if defined(__GNUC__) || defined(__clang__)
#define PO_TUI_INLINE static inline __attribute__((always_inline))
#else
#define PO_TUI_INLINE static inline
#endif
#endif

#endif /* POSTOFFICE_TUI_INTERNAL_VISIBILITY_H */
