/**
 * @file renderer.h
 * @ingroup tui_render
 * @brief Backend-agnostic renderer interface and cell surface types.
 *
 * WHAT: Defines the surface/cell model and renderer API.
 * HOW (see `renderer.c` and specific backends):
 *  - Uses a draw-batch (`draw_batch`) to coalesce commands.
 *  - `renderer_buffer` provides double-buffering for diffing.
 *  - Terminal backend (`renderer_term`) flushes to ncurses.
 */

#ifndef POSTOFFICE_TUI_RENDER_RENDERER_H
#define POSTOFFICE_TUI_RENDER_RENDERER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief One terminal cell (character + style indices).
 * For phase-1, `ch` is a single-byte code; future: wide char support.
 */
typedef struct tui_cell {
    uint32_t ch;    /**< Codepoint or ASCII */
    uint16_t fg;    /**< Theme palette index */
    uint16_t bg;    /**< Theme palette index */
    uint16_t attrs; /**< Bitmask (bold, underline, etc.) */
} tui_cell;

/**
 * @brief Linear cell surface (row-major) representing a frame.
 */
typedef struct tui_surface {
    int width;
    int height;
    tui_cell *cells;
} tui_surface;

typedef struct tui_renderer tui_renderer; /**< Opaque renderer handle */

/** Error codes for renderer operations (0 == success) */
typedef enum tui_renderer_err {
    TUI_REN_OK = 0,
    TUI_REN_EINVAL = -1,
    TUI_REN_ENOMEM = -2,
    TUI_REN_EBACKEND = -3,
} tui_renderer_err;

/** Capability flags reported by renderer */
typedef enum tui_renderer_caps {
    TUI_REN_CAP_COLOR256 = 1u << 0,
    TUI_REN_CAP_TRUECOLOR = 1u << 1,
    TUI_REN_CAP_BOLD = 1u << 2,
    TUI_REN_CAP_UNDERLINE = 1u << 3,
    TUI_REN_CAP_MOUSE = 1u << 4,
    TUI_REN_CAP_WIDECH = 1u << 5,
} tui_renderer_caps;

/** Present flags controlling flush behavior */
typedef enum tui_present_flags {
    TUI_PRESENT_FORCE_FULL = 1u << 0, /**< ignore damage, redraw all */
    TUI_PRESENT_SYNC = 1u << 1,       /**< force refresh now */
} tui_present_flags;

/** Renderer construction parameters */
typedef struct tui_renderer_config {
    int width;        /**< initial width (cells); <=0 -> detect */
    int height;       /**< initial height (cells); <=0 -> detect */
    unsigned backend; /**< 0=auto, 1=ncurses, ... (reserved) */
    unsigned flags;   /**< reserved */
} tui_renderer_config;

/** Diagnostics callback */
typedef void (*tui_log_fn)(void *ud, const char *msg);

/** Create a renderer based on runtime configuration. */
tui_renderer *tui_renderer_create(const tui_renderer_config *cfg, tui_log_fn log_cb, void *log_ud);

/** Destroy a renderer. */
void tui_renderer_destroy(tui_renderer *r);

/** Query terminal size; may differ from initial width/height. */
int tui_renderer_query_size(tui_renderer *r, int *out_w, int *out_h);

/** Get capability bitmask. */
unsigned tui_renderer_capabilities(const tui_renderer *r);

/**
 * Present a frame surface to the backend (diffed if available).
 * @param r renderer
 * @param frame surface to present
 * @param flags combination of tui_present_flags
 * @return 0 on success, <0 on error
 */
int tui_renderer_present(tui_renderer *r, const tui_surface *frame, unsigned flags);

#endif /* POSTOFFICE_TUI_RENDER_RENDERER_H */
