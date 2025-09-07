#include "unity/unity_fixture.h"
#include "ui/ncurses_dyn.h"
#include "ui/ncurses_integration.h"

#include <stdio.h>

TEST_GROUP(NCURSES_DYN);

TEST_SETUP(NCURSES_DYN) {}
TEST_TEAR_DOWN(NCURSES_DYN) {
    if (po_ncurses_ui_active()) {
        po_ncurses_ui_shutdown();
    }
    /* allow re-load for subsequent tests */
    po_ncurses_unload();
}

TEST(NCURSES_DYN, LOAD_SYMBOL_TABLE_OR_SKIP) {
    char err[256];
    int rc = po_ncurses_load(NULL, err, sizeof err);
    if (rc < 0) {
        TEST_IGNORE_MESSAGE("ncursesw not present on system; skipping dynamic loader tests");
    }
    const po_ncurses_api *api = po_ncurses();
    TEST_ASSERT_NOT_NULL(api);
    TEST_ASSERT_TRUE(api->loaded);
    TEST_ASSERT_NOT_NULL(api->initscr);
    TEST_ASSERT_NOT_NULL(api->endwin);
}

TEST(NCURSES_DYN, BOOT_AND_SHUTDOWN_UI) {
    char err[256];
    if (po_ncurses_ui_boot(PO_NCURSES_UI_FLAG_NONBLOCK | PO_NCURSES_UI_FLAG_HIDE_CURSOR | PO_NCURSES_UI_FLAG_ENABLE_COLOR, err, sizeof err) < 0) {
        TEST_IGNORE_MESSAGE("ncursesw UI boot failed (likely not installed); skipping");
    }
    TEST_ASSERT_TRUE(po_ncurses_ui_active());
    po_ncurses_ui_shutdown();
    TEST_ASSERT_FALSE(po_ncurses_ui_active());
}

TEST_GROUP_RUNNER(NCURSES_DYN) {
    RUN_TEST_CASE(NCURSES_DYN, LOAD_SYMBOL_TABLE_OR_SKIP);
    RUN_TEST_CASE(NCURSES_DYN, BOOT_AND_SHUTDOWN_UI);
}
