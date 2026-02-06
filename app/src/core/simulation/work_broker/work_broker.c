/**
 * @file work_broker.c
 * @brief Work Broker Process Entry Point.
 */

#define _POSIX_C_SOURCE 200809L

#include <getopt.h>
#include <postoffice/sysinfo/sysinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "api/broker_core.h"
#include "ipc/sim_client.h"

// Explicit declaration to workaround persistent implicit declaration error
// likely due to subtle header interaction or toolchain quirk.
void broker_shutdown(broker_ctx_t *ctx);

static broker_ctx_t g_ctx;

static void on_sig(int s, siginfo_t *i, void *c) {
    (void)s;
    (void)i;
    (void)c;
    g_ctx.shutdown_requested = 1;
}

int main(int argc, char *argv[]) {
    po_sysinfo_t sysinfo;
    po_sysinfo_collect(&sysinfo);

    char *loglevel = "INFO";
    size_t pool_size = 0;

    struct option long_opts[] = {{"pool-size", required_argument, 0, 'p'},
                                 {"loglevel", required_argument, 0, 'l'},
                                 {0, 0, 0, 0}};
    int opt;
    while ((opt = getopt_long(argc, argv, "l:p:", long_opts, NULL)) != -1) {
        if (opt == 'p')
            pool_size = (size_t)atol(optarg);
        if (opt == 'l')
            loglevel = optarg;
    }

    if (pool_size == 0) {
        pool_size = (size_t)sysinfo.physical_cores * 4;
        if (pool_size < 32)
            pool_size = 32;
    }

    // Setup signals
    sim_client_setup_signals(on_sig);

    if (broker_init(&g_ctx, loglevel, pool_size) != 0) {
        return 1;
    }

    broker_run(&g_ctx);
    broker_shutdown(&g_ctx);

    return 0;
}
