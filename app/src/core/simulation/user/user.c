/**
 * @file user.c
 * @brief User Process Entry Point.
 * 
 * Represents a client "Person" in the simulation who requests services.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <string.h>

#include "runtime/user_loop.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <postoffice/sort/sort.h>

/**
 * @brief Parses command line arguments.
 */
static void parse_cli_args(int argc, char** argv, int* user_id, int* service_type) {
    int opt;
    while ((opt = getopt(argc, argv, "l:i:s:")) != -1) {
        char *endptr;
        long val;
        switch (opt) {
            case 'l':
                if (setenv("PO_LOG_LEVEL", optarg, 1) != 0) {
                    fprintf(stderr, "Warning: Failed to set PO_LOG_LEVEL: %s\n", strerror(errno));
                }
                break;
            case 'i': 
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val >= 0) *user_id = (int)val;
                break;
            case 's': 
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val >= 0) *service_type = (int)val;
                break;
            default: break;
        }
    }
}

int main(int argc, char** argv) {
    int user_id = -1;
    int service_type = -1;

    parse_cli_args(argc, argv, &user_id, &service_type);
    po_sort_init();

    if (user_id == -1 || service_type == -1) {
        fprintf(stderr, "Usage: %s [-l <log_level>] -i <id> -s <type>\n", argv[0]);
        return 1;
    }

    sim_shm_t* shared_memory = NULL;
    if (initialize_user_runtime(&shared_memory) != 0) {
        return 1;
    }

    int result = run_user_simulation_loop(user_id, service_type, shared_memory, NULL);

    teardown_user_runtime(shared_memory);
    po_sort_finish();
    return result;
}
