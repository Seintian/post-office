#include "app_tui.h"
#include "clay/clay_core.h"
#include "screens/screen_dashboard.h"
#include <string.h>

#include "screens/screen_entities.h"
#include "screens/screen_performance.h"
#include "screens/screen_config.h"
#include "screens/screen_template.h"

void app_tui_run_simulation(void) {
    // ...
}

void app_tui_run_demo(void) {
    app_tui_run_simulation();
}

void app_tui_navigate_to(const char *screen_name) {
    if (!screen_name) return;

    // Direct routing
    if (strcmp(screen_name, "Dashboard") == 0 || strcmp(screen_name, "Director") == 0) {
        screen_dashboard_create();
    } else if (strcmp(screen_name, "Ticket Issuer") == 0) {
        screen_entities_create("Ticket Issuer", 12);
    } else if (strcmp(screen_name, "Users Manager") == 0) {
        screen_entities_create("Users Manager", 3);
    } else if (strcmp(screen_name, "Worker") == 0) {
        screen_entities_create("Worker", 5);
    } else if (strcmp(screen_name, "User") == 0) {
        screen_user_create();
    } else if (strcmp(screen_name, "Performance") == 0) {
        screen_performance_create();
    } else if (strcmp(screen_name, "Configuration") == 0) {
        screen_config_create();
    }
}
