/**
 * @file storage.c
 */

#include "storage/storage.h"

#include <stdlib.h>

#include "log/logger.h"

static po_logstore_t *g_ls = NULL;

int storage_init(const storage_config_t *cfg) {
	if (!cfg || !cfg->dir || !cfg->bucket) return -1;
	po_logstore_cfg lc = {
		.dir = cfg->dir,
		.bucket = cfg->bucket,
		.map_size = cfg->map_size,
		.ring_capacity = cfg->ring_capacity ? cfg->ring_capacity : 1024,
		.batch_size = cfg->batch_size ? cfg->batch_size : 32,
		.fsync_policy = cfg->fsync_policy,
	};
	g_ls = po_logstore_open_cfg(&lc);
	if (!g_ls) return -1;
	if (cfg->attach_logger_sink) {
		(void)po_logstore_attach_logger(g_ls);
	}
	return 0;
}

void storage_shutdown(void) {
	if (g_ls) {
		po_logstore_close(&g_ls);
		g_ls = NULL;
	}
}

po_logstore_t *storage_logstore(void) {
	return g_ls;
}

