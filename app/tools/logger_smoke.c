#include <stdio.h>
#include <unistd.h>
#include <postoffice/log/logger.h>

int main(void) {
    logger_config cfg = {
        .level = LOG_DEBUG,
        .ring_capacity = 1u << 12,
        .consumers = 1,
        .policy = LOGGER_OVERWRITE_OLDEST,
    };
    if (logger_init(&cfg) != 0) {
        fprintf(stderr, "logger: init failed\n");
        return 1;
    }
    logger_add_sink_console(true);

    LOG_INFO("logger smoke test started pid=%d", (int)getpid());
    LOG_DEBUG("debug=%d", 42);
    LOG_WARN("warn: %s", "sample");
    LOG_ERROR("error: code=%d", -1);

    logger_shutdown();
    return 0;
}
