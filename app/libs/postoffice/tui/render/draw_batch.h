/**
 * @file draw_batch.h
 * @ingroup tui_render
 * @brief WHAT: Draw command batching API (quads, runs, text).
 *
 * HOW (see `draw_batch.c`):
 *  - Aggregates adjacent cells with same style into runs to minimize flushes.
 *  - Provides a simple command stream useful for alternate backends/tests.
 */

#ifndef POSTOFFICE_TUI_RENDER_DRAW_BATCH_H
#define POSTOFFICE_TUI_RENDER_DRAW_BATCH_H

#include <stddef.h>
#include <stdint.h>

struct draw_batch; /* opaque */

typedef struct tui_region {
    int x, y, w, h;
} tui_region;

typedef enum draw_cmd_type {
    DRAW_CMD_CLEAR = 1,
    DRAW_CMD_CELL_RUN,
    DRAW_CMD_TEXT,
    DRAW_CMD_BOX,
} draw_cmd_type;

typedef struct draw_style {
    unsigned fg;
    unsigned bg;
    unsigned attrs; /* renderer attrs bitmask */
} draw_style;

typedef struct draw_cmd_cell_run {
    int x, y;               /* start */
    int len;                /* cells */
    draw_style style;       /* applied to cells */
    const uint32_t *glyphs; /* optional; if NULL, spaces */
} draw_cmd_cell_run;

typedef struct draw_cmd_text {
    int x, y;
    draw_style style;
    const char *utf8; /* null-terminated */
} draw_cmd_text;

typedef struct draw_cmd_box {
    tui_region r;
    draw_style style;
    unsigned border_mask; /* TLBR+corners bits */
} draw_cmd_box;

struct draw_batch *draw_batch_create(void);
void draw_batch_destroy(struct draw_batch *db);

void draw_batch_clear(struct draw_batch *db, draw_style style);
void draw_batch_emit_run(struct draw_batch *db, const draw_cmd_cell_run *run);
void draw_batch_emit_text(struct draw_batch *db, const draw_cmd_text *tx);
void draw_batch_emit_box(struct draw_batch *db, const draw_cmd_box *bx);

/* Introspection for tests/backends */
size_t draw_batch_count(const struct draw_batch *db);
draw_cmd_type draw_batch_cmd_type(const struct draw_batch *db, size_t idx);

#endif /* POSTOFFICE_TUI_RENDER_DRAW_BATCH_H */
