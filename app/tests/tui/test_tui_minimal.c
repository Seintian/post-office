#include <string.h>

#include "tui/ui.h"
#include "unity/unity_fixture.h"

TEST_GROUP(TUI);

TEST_SETUP(TUI) {
}
TEST_TEAR_DOWN(TUI) {
}

TEST(TUI, INIT_AND_SNAPSHOT_EMPTY) {
    po_tui_app *app = NULL;
    int rc = po_tui_init(&app, NULL);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_NULL(app);
    char buf[64];
    rc = po_tui_render(app);
    TEST_ASSERT_EQUAL_INT(0, rc);
    rc = po_tui_snapshot(app, buf, sizeof(buf), NULL);
    TEST_ASSERT_EQUAL_INT(0, rc);
    for (size_t i = 0; i < sizeof(buf) && buf[i]; ++i) {
        TEST_ASSERT(buf[i] == ' ' || buf[i] == '\n');
    }
    po_tui_shutdown(app);
}

TEST(TUI, ADD_LABEL_AND_SNAPSHOT) {
    po_tui_config cfg = {.width_override = 20, .height_override = 5};
    po_tui_app *app = NULL;
    TEST_ASSERT_EQUAL_INT(0, po_tui_init(&app, &cfg));
    TEST_ASSERT_GREATER_OR_EQUAL(0, po_tui_add_label(app, 2, 1, "Hello"));
    TEST_ASSERT_EQUAL_INT(0, po_tui_render(app));
    char snap[256];
    TEST_ASSERT_EQUAL_INT(0, po_tui_snapshot(app, snap, sizeof(snap), NULL));
    const char *second = strchr(snap, '\n');
    TEST_ASSERT_NOT_NULL(second);
    second++; // move to start of second line
    char expected_line[21];
    memset(expected_line, ' ', 20);
    memcpy(expected_line + 2, "Hello", 5);
    expected_line[20] = '\0';
    TEST_ASSERT_EQUAL_MEMORY(expected_line, second, 20);
    po_tui_shutdown(app);
}

TEST_GROUP_RUNNER(TUI) {
    RUN_TEST_CASE(TUI, INIT_AND_SNAPSHOT_EMPTY);
    RUN_TEST_CASE(TUI, ADD_LABEL_AND_SNAPSHOT);
}