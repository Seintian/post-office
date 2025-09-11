// Minimal phase-1 implementation of public TUI API (scaffold)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "tui/ui.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Opaque structures
struct po_tui_label {
    int x, y;
    char *text;
};

struct po_tui_app {
    int width;
    int height;
    struct po_tui_label *labels;
    size_t labels_count;
    size_t labels_cap;
    // off-screen buffer (width * height chars)
    char *buffer;
};

static int ensure_label_capacity(struct po_tui_app *app) {
    if (app->labels_count < app->labels_cap)
        return 0;

    size_t new_cap = app->labels_cap ? app->labels_cap * 2 : 8;
    void *p = realloc(app->labels, new_cap * sizeof(*app->labels));
    if (!p)
        return -1;

    app->labels = (struct po_tui_label *)p;
    app->labels_cap = new_cap;
    return 0;
}

int po_tui_init(po_tui_app **out_app, const po_tui_config *cfg) {
    if (!out_app)
        return -1;

    *out_app = NULL;
    po_tui_app *app = (po_tui_app *)calloc(1, sizeof(po_tui_app));
    if (!app)
        return -1;

    int w = 80, h = 24; // defaults
    if (cfg) {
        if (cfg->width_override > 0)
            w = cfg->width_override;
        if (cfg->height_override > 0)
            h = cfg->height_override;
    }
    if (w <= 0 || h <= 0) {
        free(app);
        return -1;
    }

    app->width = w;
    app->height = h;
    app->buffer = malloc((size_t)w * (size_t)h);
    if (!app->buffer) {
        free(app);
        return -1;
    }

    memset(app->buffer, ' ', (size_t)w * (size_t)h);
    *out_app = app;
    return 0;
}

void po_tui_shutdown(po_tui_app *app) {
    if (!app)
        return;

    for (size_t i = 0; i < app->labels_count; ++i) free(app->labels[i].text);
    free(app->labels);
    free(app->buffer);
    free(app);
}

int po_tui_add_label(po_tui_app *app, int x, int y, const char *text) {
    if (!app || !text)
        return -1;

    if (x < 0 || y < 0 || x >= app->width || y >= app->height)
        return -1;

    if (ensure_label_capacity(app) != 0)
        return -1;

    struct po_tui_label *lbl = &app->labels[app->labels_count];
    lbl->x = x;
    lbl->y = y;
    lbl->text = strdup(text);
    if (!lbl->text)
        return -1;

    return (int)app->labels_count++;
}

int po_tui_render(po_tui_app *app) {
    if (!app)
        return -1;

    // Clear buffer
    memset(app->buffer, ' ', (size_t)app->width * (size_t)app->height);
    for (size_t i = 0; i < app->labels_count; ++i) {
        struct po_tui_label *lbl = &app->labels[i];
        if (!lbl->text)
            continue;

        size_t len = strlen(lbl->text);
        if (lbl->y < 0 || lbl->y >= app->height)
            continue;

        int x = lbl->x;
        for (size_t c = 0; c < len; ++c) {
            int cx = x + (int)c;
            if (cx < 0)
                continue;
            if (cx >= app->width)
                break;

            app->buffer[lbl->y * app->width + cx] = lbl->text[c];
        }
    }

    return 0;
}

int po_tui_snapshot(const po_tui_app *app, char *out, size_t out_size, size_t *written) {
    if (!app || !out || out_size == 0)
        return -1;

    size_t need = (size_t)app->height * ((size_t)app->width + 1); // +1 for each newline
    size_t produced = 0;
    for (int row = 0; row < app->height; ++row) {
        size_t line_len = (size_t)app->width;
        if (produced + line_len + 1 >= out_size) { // reserve space for NUL
            line_len = out_size - produced - 2; // -1 newline -1 NUL
            if ((ssize_t)line_len < 0)
                line_len = 0;
        }

        memcpy(out + produced, app->buffer + row * app->width, line_len);
        produced += line_len;
        if (row != app->height - 1 && produced + 1 < out_size) {
            out[produced++] = '\n';
        }
        if (produced + 1 >= out_size)
            break;
    }

    out[produced] = '\0';
    if (written)
        *written = produced;

    (void)need; // reserved for future diagnostics
    return 0;
}
