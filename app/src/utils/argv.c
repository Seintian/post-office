#include "utils/argv.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void po_args_init(po_args_t *args) {
    args->help = false;
    args->version = false;
    args->config_file = NULL;
    args->loglevel = 0;
    args->syslog = false;
    args->syslog_ident = NULL;
    args->tui_demo = false;
    args->tui_sim = false;
    args->fd = -1;
}

void po_args_print_usage(int fd, const char *prog_name) {
    dprintf(fd, "Usage: %s [options]\n", prog_name);
    dprintf(fd,
            "  -h, --help            Show this help message and exit\n"
            "  -v, --version         Show version information and exit\n"
            "  -c, --config FILE     Path to configuration file\n"
            "  -l, --loglevel LEVEL  Set log level (TRACE|DEBUG|INFO|WARN|ERROR|FATAL or 0..5)\n"
            "      --syslog          Enable syslog logging sink\n"
            "      --syslog-ident S  Set syslog ident (default: 'postoffice')\n"
            "      --tui             Run with TUI (Terminal User Interface)\n"
            "      --tui-demo        Run a minimal TUI smoke demo and exit\n");
}

void po_args_destroy(po_args_t *args) {
    free(args->config_file);
    args->config_file = NULL;
    free(args->syslog_ident);
    args->syslog_ident = NULL;
}

static int parse_loglevel(const char *arg, int *out_lvl) {
    if (!arg) return -1;

    struct {
        const char *name;
        int value;
    } map[] = {{"TRACE", 0}, {"DEBUG", 1}, {"INFO", 2}, {"WARN", 3}, {"WARNING", 3},
               {"ERROR", 4}, {"FATAL", 5}, {NULL, -1}};

    for (int i = 0; map[i].name; ++i) {
        if (strcasecmp(arg, map[i].name) == 0) {
            *out_lvl = map[i].value;
            return 0;
        }
    }

    char *endptr = NULL;
    long v = strtol(arg, &endptr, 10);
    if (*endptr != '\0' || v < 0 || v > 5) return -1;
    *out_lvl = (int)v;
    return 0;
}

static int po_handle_option(int opt, po_args_t *args, int fd, const char *prog_name, const char *optarg_value) {
    switch (opt) {
    case 'h':
        args->help = true;
        po_args_print_usage(fd, prog_name);
        return 1;
    case 'v':
        args->version = true;
        dprintf(fd, "%s version %s\n", prog_name, "1.0.0");
        return 1;
    case 'c':
        if (!optarg_value) {
            fprintf(stderr, "Option -c/--config requires a non-empty argument\n");
            po_args_print_usage(fd, prog_name);
            return -1;
        }
        free(args->config_file);
        args->config_file = strdup(optarg_value);
        if (!args->config_file) {
            fprintf(stderr, "Failed to allocate memory for config path\n");
            return -1;
        }
        return 0;
    case 'l': {
        int lvl;
        if (!optarg_value || parse_loglevel(optarg_value, &lvl) != 0) {
            fprintf(stderr, "Invalid log level: %s\n", optarg_value ? optarg_value : "(null)");
            po_args_print_usage(fd, prog_name);
            return -1;
        }
        args->loglevel = lvl;
        return 0;
    }
    case 1: // --syslog
        args->syslog = true;
        return 0;
    case 2: // --syslog-ident
        free(args->syslog_ident);
        args->syslog_ident = optarg_value ? strdup(optarg_value) : NULL;
        if (optarg_value && !args->syslog_ident) {
            fprintf(stderr, "Failed to allocate memory for syslog ident\n");
            return -1;
        }
        return 0;
    case 3: // --tui-demo
        args->tui_demo = true;
        return 0;
    case 4: // --tui
        args->tui_sim = true;
        return 0;
    case '?':
    default:
        po_args_print_usage(fd, prog_name);
        return -1;
    }
}

int po_args_parse(po_args_t *args, int argc, char **argv, int fd) {
    static struct option long_opts[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"loglevel", required_argument, NULL, 'l'},
        {"config", required_argument, NULL, 'c'},
        {"syslog", no_argument, NULL, 1},
        {"syslog-ident", required_argument, NULL, 2},
        {"tui-demo", no_argument, NULL, 3},
        {"tui", no_argument, NULL, 4},
        {NULL, 0, NULL, 0}
    };

    int opt = 0;
    int opt_index = 0;
    while ((opt = getopt_long(argc, argv, "hvc:l:", long_opts, &opt_index)) != -1) {
        int status = po_handle_option(opt, args, fd, argv[0], optarg);
        if (status != 0) {
            return status;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        po_args_print_usage(fd, argv[0]);
        return -1;
    }

    if (args->loglevel < 0 || args->loglevel > 5) {
        fprintf(stderr, "Invalid log level: %d\n", args->loglevel);
        po_args_print_usage(fd, argv[0]);
        return -1;
    }

    return 0;
}
