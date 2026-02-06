/**
 * @file worker.c
 * @brief Worker Process Entry Point.
 */

#define _POSIX_C_SOURCE 200809L

#include "worker_core.h"

int main(int argc, char **argv) {
    worker_config_t cfg;
    worker_parse_args(argc, argv, &cfg);

    return worker_run_simulation(&cfg);
}
