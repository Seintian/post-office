#include <postoffice/log/logger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    logger_level_t level = LOG_DEBUG;

    const char *lvl = getenv("LOG_LEVEL");
    if (lvl) {
        if (strcasecmp(lvl, "TRACE") == 0)
            level = LOG_TRACE;

        else if (strcasecmp(lvl, "DEBUG") == 0)
            level = LOG_DEBUG;

        else if (strcasecmp(lvl, "INFO") == 0)
            level = LOG_INFO;

        else if (strcasecmp(lvl, "WARN") == 0 || strcasecmp(lvl, "WARNING") == 0)
            level = LOG_WARN;

        else if (strcasecmp(lvl, "ERROR") == 0)
            level = LOG_ERROR;

        else if (strcasecmp(lvl, "FATAL") == 0)
            level = LOG_FATAL;
    }

    po_logger_config_t cfg = {
        .level = level,
        .ring_capacity = 1u << 12,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
    };
    if (po_logger_init(&cfg) != 0) {
        fprintf(stderr, "logger: init failed\n");
        return 1;
    }

    po_logger_add_sink_console(true);

    const char *use_syslog = getenv("SYSLOG");
    if (use_syslog && *use_syslog == '1') {
        const char *ident = getenv("SYSLOG_IDENT");
    po_logger_add_sink_syslog(ident && *ident ? ident : NULL);
    }

    LOG_INFO("logger smoke test started pid=%d", getpid());
    LOG_DEBUG("debug=%d", 42);
    LOG_WARN("warn: %s", "sample");
    LOG_ERROR("error: code=%d", -1);

    po_logger_shutdown();
    return 0;
}
