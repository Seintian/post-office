#include <postoffice/log/logger.h>
#include <postoffice/utils/argv.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    po_args_t args;
    po_args_init(&args);
    int rc = po_args_parse(&args, argc, argv, STDERR_FILENO);
    if (rc != 0) {
        // help/version already printed when rc==1; error printed on rc<0
        return rc < 0 ? 1 : 0;
    }

    // Initialize logger according to CLI
    logger_config cfg = {
        .level = (args.loglevel >= 0 && args.loglevel <= 5) ? (logger_level_t)args.loglevel : LOG_INFO,
        .ring_capacity = 1u << 14,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
    };
    if (logger_init(&cfg) != 0) {
        fprintf(stderr, "logger: init failed\n");
        return 1;
    }
    // Always log to console by default
    logger_add_sink_console(true);
    if (args.syslog) {
        logger_add_sink_syslog(args.syslog_ident);
    }

    LOG_INFO("post-office main started (level=%d)%s", (int)logger_get_level(), args.syslog ? " with syslog" : "");

    // TODO: wire configuration file and launch application subsystems

    logger_shutdown();
    return 0;
}
