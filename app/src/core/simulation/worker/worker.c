#define _POSIX_C_SOURCE 200809L

#include "runtime/worker_loop.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int worker_id = -1;
    int service_type = -1;

    int opt;
    while ((opt = getopt(argc, argv, "l:i:s:")) != -1) {
        char *endptr;
        long val;
        switch (opt) {
            case 'l':
                setenv("PO_LOG_LEVEL", optarg, 1);
                break;
            case 'i': 
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val >= 0) worker_id = (int)val;
                break;
            case 's': 
                val = strtol(optarg, &endptr, 10);
                if (*endptr == '\0' && val >= 0) service_type = (int)val;
                break;
            default: break;
        }
    }

    if (worker_id == -1 || service_type == -1) {
        fprintf(stderr, "Usage: %s -i <worker_id> -s <service_type>\n", argv[0]);
        return 1;
    }

    return worker_run(worker_id, service_type);
}
