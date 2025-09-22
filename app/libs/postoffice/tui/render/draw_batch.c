/**
 * @file draw_batch.c
 * @ingroup tui_render
 * @brief HOW: Draw command batching implementation.
 */

#include "render/draw_batch.h"

#include <stdlib.h>
#include <string.h>

typedef struct draw_cmd_any {
    draw_cmd_type type;
    union {
        draw_cmd_cell_run run;
        draw_cmd_text text;
        draw_cmd_box box;
    } as;
} draw_cmd_any;

struct draw_batch {
    draw_cmd_any *cmds;
    size_t count;
    size_t cap;
    draw_style clear_style;
};

static int ensure_cap(struct draw_batch *db) {
    if (db->count < db->cap)
        return 0;
    size_t nc = db->cap ? db->cap * 2 : 64;
    void *p = realloc(db->cmds, nc * sizeof(draw_cmd_any));
    if (!p)
        return -1;
    db->cmds = (draw_cmd_any *)p;
    db->cap = nc;
    return 0;
}

struct draw_batch *draw_batch_create(void) {
    return (struct draw_batch *)calloc(1, sizeof(struct draw_batch));
}
void draw_batch_destroy(struct draw_batch *db) {
    if (!db)
        return;
    free(db->cmds);
    free(db);
}

void draw_batch_clear(struct draw_batch *db, draw_style style) {
    if (!db)
        return;
    db->count = 0;
    db->clear_style = style;
}
void draw_batch_emit_run(struct draw_batch *db, const draw_cmd_cell_run *run) {
    if (!db || !run)
        return;
    if (ensure_cap(db) != 0)
        return;
    draw_cmd_any *c = &db->cmds[db->count++];
    c->type = DRAW_CMD_CELL_RUN;
    c->as.run = *run;
}
void draw_batch_emit_text(struct draw_batch *db, const draw_cmd_text *tx) {
    if (!db || !tx)
        return;
    if (ensure_cap(db) != 0)
        return;
    draw_cmd_any *c = &db->cmds[db->count++];
    c->type = DRAW_CMD_TEXT;
    c->as.text = *tx;
}
void draw_batch_emit_box(struct draw_batch *db, const draw_cmd_box *bx) {
    if (!db || !bx)
        return;
    if (ensure_cap(db) != 0)
        return;
    draw_cmd_any *c = &db->cmds[db->count++];
    c->type = DRAW_CMD_BOX;
    c->as.box = *bx;
}

size_t draw_batch_count(const struct draw_batch *db) {
    return db ? db->count : 0;
}
draw_cmd_type draw_batch_cmd_type(const struct draw_batch *db, size_t idx) {
    return (!db || idx >= db->count) ? 0 : db->cmds[idx].type;
}
/**
 * @file draw_batch.c
 * @ingroup tui_render
 * @brief HOW: Draw batch building and coalescing prior to backend flush.
 *
 * Batching strategy:
 *  - Merge adjacent cells with identical style into spans.
 *  - Use `perf` batcher to accumulate and emit in cache-friendly order.
 */

// Implementation will be added in subsequent milestones.
