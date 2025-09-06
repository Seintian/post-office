#include "utils/argv.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void po_args_init(po_args_t *args) {
    args->help = false;
    args->version = false;
    args->config_file = NULL;
    args->loglevel = 0;
    args->syslog = false;
    args->syslog_ident = NULL;
}

void po_args_print_usage(int fd, const char *prog_name) {
    dprintf(fd, "Usage: %s [options]\n", prog_name);
    dprintf(fd,
            "  -h, --help            Show this help message and exit\n"
            "  -v, --version         Show version information and exit\n"
            "  -c, --config FILE     Path to configuration file\n"
            "  -l, --loglevel LEVEL  Set log level (TRACE|DEBUG|INFO|WARN|ERROR|FATAL or 0..5)\n"
            "      --syslog          Enable syslog logging sink\n"
            "      --syslog-ident S  Set syslog ident (default: 'postoffice')\n");
}

void po_args_destroy(po_args_t *args) {
    if (!args)
        return;
    free(args->config_file);
    args->config_file = NULL;
    free(args->syslog_ident);
    args->syslog_ident = NULL;
}

int po_args_parse(po_args_t *args, int argc, char **argv, int fd) {
    static struct option long_opts[] = {{"help", no_argument, NULL, 'h'},
                                        {"version", no_argument, NULL, 'v'},
                                        {"loglevel", required_argument, NULL, 'l'},
                                        {"config", required_argument, NULL, 'c'},
                                        {"syslog", no_argument, NULL, 1},
                                        {"syslog-ident", required_argument, NULL, 2},
                                        {NULL, 0, NULL, 0}};

    int opt;
    int opt_index = 0;
    while ((opt = getopt_long(argc, argv, "hvc:l:", long_opts, &opt_index)) != -1) {
        switch (opt) {
        case 'h':
            args->help = true;
            po_args_print_usage(fd, argv[0]);
            return 1;

        case 'v':
            args->version = true;
            dprintf(fd, "%s version %s\n", argv[0], "1.0.0");
            return 1;

        case 'c':
            args->config_file = strdup(optarg);
            break;

        case 'l': {
            // Accept both numeric (0..5) and symbolic names
            if (optarg) {
                if (strcasecmp(optarg, "TRACE") == 0)
                    args->loglevel = 0;

                else if (strcasecmp(optarg, "DEBUG") == 0)
                    args->loglevel = 1;

                else if (strcasecmp(optarg, "INFO") == 0)
                    args->loglevel = 2;

                else if (strcasecmp(optarg, "WARN") == 0 || strcasecmp(optarg, "WARNING") == 0)
                    args->loglevel = 3;

                else if (strcasecmp(optarg, "ERROR") == 0)
                    args->loglevel = 4;

                else if (strcasecmp(optarg, "FATAL") == 0)
                    args->loglevel = 5;

                else
                    args->loglevel = atoi(optarg);
            }
            break;
        }

        case 1: // --syslog
            args->syslog = true;
            break;

        case 2: // --syslog-ident
            free(args->syslog_ident);
            args->syslog_ident = strdup(optarg);
            break;

        case '?':
        default:
            po_args_print_usage(fd, argv[0]);
            return -1;
        }
    }

    // Check for remaining non-option arguments
    if (optind < argc) {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        po_args_print_usage(fd, argv[0]);
        return -1;
    }

    // loglevel must have been set to a valid value
    if (args->loglevel < 0) {
        fprintf(stderr, "Invalid log level: %d\n", args->loglevel);
        po_args_print_usage(fd, argv[0]);
        return -1;
    }

    return 0;
}
