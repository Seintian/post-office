#include "ui/ncurses_integration.h"
#include "ui/ncurses_dyn.h"

#include <stdio.h>
#include <string.h>

static int g_ui_active = 0;

int po_ncurses_ui_boot(unsigned flags, char *errbuf, size_t errsz) {
    if (g_ui_active) return 0; /* already */

    char lerr[256];
    if (po_ncurses_load(NULL, lerr, sizeof lerr) < 0) {
        if (errbuf && errsz) snprintf(errbuf, errsz, "%s", lerr);
        return -1;
    }
    const po_ncurses_api *api = po_ncurses();
    if (!api) {
        if (errbuf && errsz) snprintf(errbuf, errsz, "ncurses API unavailable after load");
        return -1;
    }

    WINDOW *stdw = api->initscr();
    if (!stdw) {
        if (errbuf && errsz) snprintf(errbuf, errsz, "initscr failed");
        return -1;
    }

    /* raw mode features */
    if (api->noecho) api->noecho();
    if (api->cbreak) api->cbreak();
    if (flags & PO_NCURSES_UI_FLAG_HIDE_CURSOR) {
        if (api->curs_set) api->curs_set(0);
    }
    if (flags & PO_NCURSES_UI_FLAG_NONBLOCK) {
        if (api->nodelay) api->nodelay(stdw, true);
    }
    if (api->keypad) api->keypad(stdw, true);

    if ((flags & PO_NCURSES_UI_FLAG_ENABLE_COLOR) && api->has_colors && api->has_colors()) {
        if (api->start_color) api->start_color();
        if (api->use_default_colors) api->use_default_colors();
        if (api->init_pair) {
            /* Provide a couple of default pairs; ignore failures */
            api->init_pair(1, 2 /*green*/, -1);
            api->init_pair(2, 1 /*red*/, -1);
        }
    }

    g_ui_active = 1;
    return 0;
}

void po_ncurses_ui_shutdown(void) {
    if (!g_ui_active) return;
    const po_ncurses_api *api = po_ncurses();
    if (api && api->endwin) api->endwin();
    g_ui_active = 0;
}

bool po_ncurses_ui_active(void) { return g_ui_active != 0; }
