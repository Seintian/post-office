/**
 * @file logstore.c
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "storage/logstore.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>

#include "log/logger.h"
#include "perf/ringbuf.h"
#include "perf/batcher.h"
#include "storage/db_lmdb.h"
#include "storage/index.h"

typedef struct {
	void *k;
	size_t klen;
	void *v;
	size_t vlen;
} append_req_t;

struct po_logstore {
	int fd; // append-only file descriptor
	db_env_t *env;
	db_bucket_t *idx; // LMDB bucket used as index key -> (offset,len) packed
	perf_ringbuf_t *q;
	perf_batcher_t *b;
	pthread_t worker;
	_Atomic int running;
	// fast-path in-memory index guarded by RW lock
	po_index_t *mem_idx;
	pthread_rwlock_t idx_lock;
	size_t batch_size;
	po_logstore_fsync_policy_t fsync_policy;
	_Atomic uint64_t seq;
};

static void *worker_main(void *arg) {
	po_logstore_t *ls = (po_logstore_t *)arg;
	void **batch = malloc(sizeof(void*) * ls->batch_size);
	if (!batch) return NULL;
	while (atomic_load(&ls->running)) {
		ssize_t n = perf_batcher_next(ls->b, batch);
		if (n < 0) {
			// transient error; idle
			struct timespec ts = { .tv_sec = 0, .tv_nsec = 1 * 1000 * 1000 };
			nanosleep(&ts, NULL);
			continue;
		}
		// compute base offset and build iovecs
		off_t base = lseek(ls->fd, 0, SEEK_END);
		if (base < 0) {
			LOG_ERROR("logstore: lseek failed");
			base = 0;
		}
		size_t iov_cnt = (size_t)n * 4u;
		struct iovec *iov = (struct iovec *)malloc(sizeof(struct iovec) * iov_cnt);
		if (!iov) {
			// fallback: process one by one
			for (ssize_t i = 0; i < n; ++i) {
				append_req_t *req = (append_req_t *)batch[i];
				uint32_t kl = (uint32_t)req->klen, vl = (uint32_t)req->vlen;
				off_t off = lseek(ls->fd, 0, SEEK_END);
				if (off < 0) off = 0;
				ssize_t wr;
				wr = write(ls->fd, &kl, sizeof(kl)); if (wr != (ssize_t)sizeof(kl)) { LOG_ERROR("logstore: write kl failed"); }
				wr = write(ls->fd, &vl, sizeof(vl)); if (wr != (ssize_t)sizeof(vl)) { LOG_ERROR("logstore: write vl failed"); }
				wr = write(ls->fd, req->k, req->klen); if (wr != (ssize_t)req->klen) { LOG_ERROR("logstore: write key failed"); }
				wr = write(ls->fd, req->v, req->vlen); if (wr != (ssize_t)req->vlen) { LOG_ERROR("logstore: write val failed"); }
				uint8_t iv[12];
				memcpy(iv, &off, 8); memcpy(iv + 8, &vl, 4);
				(void)db_put(ls->idx, req->k, req->klen, iv, sizeof iv);
				pthread_rwlock_wrlock(&ls->idx_lock);
				(void)po_index_put(ls->mem_idx, req->k, req->klen, (uint64_t)off, vl);
				pthread_rwlock_unlock(&ls->idx_lock);
				free(req->k); free(req->v); free(req);
			}
			if (ls->fsync_policy == PO_LS_FSYNC_EACH_BATCH) fsync(ls->fd);
			continue;
		}
		// fill iovecs and compute offsets for index updates
		uint64_t cur = (uint64_t)base;
		uint64_t *offs = (uint64_t *)malloc(sizeof(uint64_t) * (size_t)n);
		uint32_t *lens = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)n);
		if (!offs || !lens) {
			free(iov); free(offs); free(lens);
			// fallback single as above
			for (ssize_t i = 0; i < n; ++i) {
				append_req_t *req = (append_req_t *)batch[i];
				uint32_t kl = (uint32_t)req->klen, vl = (uint32_t)req->vlen;
				off_t off = lseek(ls->fd, 0, SEEK_END);
				if (off < 0) off = 0;
				ssize_t wr;
				wr = write(ls->fd, &kl, sizeof(kl)); if (wr != (ssize_t)sizeof(kl)) { LOG_ERROR("logstore: write kl failed"); }
				wr = write(ls->fd, &vl, sizeof(vl)); if (wr != (ssize_t)sizeof(vl)) { LOG_ERROR("logstore: write vl failed"); }
				wr = write(ls->fd, req->k, req->klen); if (wr != (ssize_t)req->klen) { LOG_ERROR("logstore: write key failed"); }
				wr = write(ls->fd, req->v, req->vlen); if (wr != (ssize_t)req->vlen) { LOG_ERROR("logstore: write val failed"); }
				uint8_t iv[12];
				memcpy(iv, &off, 8); memcpy(iv + 8, &vl, 4);
				(void)db_put(ls->idx, req->k, req->klen, iv, sizeof iv);
				pthread_rwlock_wrlock(&ls->idx_lock);
				(void)po_index_put(ls->mem_idx, req->k, req->klen, (uint64_t)off, vl);
				pthread_rwlock_unlock(&ls->idx_lock);
				free(req->k); free(req->v); free(req);
			}
			if (ls->fsync_policy == PO_LS_FSYNC_EACH_BATCH) fsync(ls->fd);
			continue;
		}
		size_t j = 0;
		for (ssize_t i = 0; i < n; ++i) {
			append_req_t *req = (append_req_t *)batch[i];
			uint32_t kl = (uint32_t)req->klen, vl = (uint32_t)req->vlen;
			offs[i] = cur;
			lens[i] = vl;
			iov[j++] = (struct iovec){ .iov_base = &kl, .iov_len = sizeof(kl) };
			cur += sizeof(kl);
			iov[j++] = (struct iovec){ .iov_base = &vl, .iov_len = sizeof(vl) };
			cur += sizeof(vl);
			iov[j++] = (struct iovec){ .iov_base = req->k, .iov_len = req->klen };
			cur += (uint64_t)req->klen;
			iov[j++] = (struct iovec){ .iov_base = req->v, .iov_len = req->vlen };
			cur += (uint64_t)req->vlen;
		}
		ssize_t w = pwritev(ls->fd, iov, (int)iov_cnt, base);
		if (w < 0) {
			LOG_ERROR("logstore: pwritev failed");
		} else {
			// index updates
			for (ssize_t i = 0; i < n; ++i) {
				append_req_t *req = (append_req_t *)batch[i];
				uint8_t iv[12];
				memcpy(iv, &offs[i], 8); memcpy(iv + 8, &lens[i], 4);
				(void)db_put(ls->idx, req->k, req->klen, iv, sizeof iv);
				pthread_rwlock_wrlock(&ls->idx_lock);
				(void)po_index_put(ls->mem_idx, req->k, req->klen, offs[i], lens[i]);
				pthread_rwlock_unlock(&ls->idx_lock);
			}
			if (ls->fsync_policy == PO_LS_FSYNC_EACH_BATCH) {
				(void)fsync(ls->fd);
			}
		}
		for (ssize_t i = 0; i < n; ++i) {
			append_req_t *req = (append_req_t *)batch[i];
			free(req->k); free(req->v); free(req);
		}
		free(iov); free(offs); free(lens);
	}
	free(batch);
	return NULL;
}

po_logstore_t *po_logstore_open_cfg(const po_logstore_cfg *cfg) {
	if (!cfg || !cfg->dir || !cfg->bucket) return NULL;
	char path[512];
	snprintf(path, sizeof(path), "%s/aof.log", cfg->dir);
	int fd = open(path, O_CREAT | O_WRONLY, 0664);
	if (fd < 0) {
		LOG_ERROR("logstore: open(%s) failed", path);
		return NULL;
	}

	db_env_t *env = NULL;
	if (db_env_open(cfg->dir, 8, cfg->map_size, &env) != 0) {
		LOG_ERROR("logstore: db_env_open failed");
		close(fd);
		return NULL;
	}
	db_bucket_t *idx = NULL;
	if (db_bucket_open(env, cfg->bucket, &idx) != 0) {
		LOG_ERROR("logstore: db_bucket_open(%s) failed", cfg->bucket);
		db_env_close(&env);
		close(fd);
		return NULL;
	}

	po_logstore_t *ls = (po_logstore_t *)calloc(1, sizeof(*ls));
	if (!ls) { db_bucket_close(&idx); db_env_close(&env); close(fd); return NULL; }
	ls->fd = fd;
	ls->env = env;
	ls->idx = idx;
	size_t cap = cfg->ring_capacity ? cfg->ring_capacity : 1024;
	ls->q = perf_ringbuf_create(cap);
	if (!ls->q) { po_logstore_close(&ls); return NULL; }
	ls->b = perf_batcher_create(ls->q, cfg->batch_size ? cfg->batch_size : 32);
	if (!ls->b) { po_logstore_close(&ls); return NULL; }
	ls->mem_idx = po_index_create(1024);
	if (!ls->mem_idx) { po_logstore_close(&ls); return NULL; }
	pthread_rwlock_init(&ls->idx_lock, NULL);
	ls->batch_size = cfg->batch_size ? cfg->batch_size : 32;
	ls->fsync_policy = cfg->fsync_policy;
	atomic_store(&ls->seq, 0);
	// preload in-memory index from LMDB to speed up reads
	struct preload_ud { po_index_t *idx; } u = { .idx = ls->mem_idx };
	auto int preload_cb(const void *k, size_t klen, const void *v, size_t vlen, void *ud) {
		(void)ud;
		if (vlen == 12) {
			uint64_t off;
			uint32_t len;
			memcpy(&off, v, 8);
			memcpy(&len, (const uint8_t *)v + 8, 4);
			(void)po_index_put(((struct preload_ud *)ud)->idx, k, klen, off, len);
		}
		return 0;
	}
	(void)db_iterate(ls->idx, preload_cb, &u);
	atomic_store(&ls->running, 1);
	if (pthread_create(&ls->worker, NULL, worker_main, ls) != 0) {
		po_logstore_close(&ls);
		return NULL;
	}
	return ls;
}

po_logstore_t *po_logstore_open(const char *dir, const char *bucket, size_t map_size,
								size_t ring_capacity) {
	po_logstore_cfg cfg = {
		.dir = dir,
		.bucket = bucket,
		.map_size = map_size,
		.ring_capacity = ring_capacity,
		.batch_size = 32,
		.fsync_policy = PO_LS_FSYNC_NONE,
	};
	return po_logstore_open_cfg(&cfg);
}

void po_logstore_close(po_logstore_t **pls) {
	if (!*pls) return;
	po_logstore_t *ls = *pls;
	atomic_store(&ls->running, 0);
	pthread_join(ls->worker, NULL);
	if (ls->b) perf_batcher_destroy(&ls->b);
	if (ls->q) perf_ringbuf_destroy(&ls->q);
	if (ls->idx) db_bucket_close(&ls->idx);
	if (ls->env) db_env_close(&ls->env);
	if (ls->fd >= 0) close(ls->fd);
	if (ls->mem_idx) po_index_destroy(&ls->mem_idx);
	pthread_rwlock_destroy(&ls->idx_lock);
	free(ls);
	*pls = NULL;
}

int po_logstore_append(po_logstore_t *ls, const void *key, size_t keylen,
					   const void *val, size_t vallen) {
	append_req_t *r = (append_req_t *)calloc(1, sizeof(*r));
	if (!r) return -1;
	r->k = malloc(keylen);
	r->v = malloc(vallen);
	if (!r->k || !r->v) { free(r->k); free(r->v); free(r); return -1; }
	memcpy(r->k, key, keylen);
	memcpy(r->v, val, vallen);
	r->klen = keylen;
	r->vlen = vallen;
	if (perf_ringbuf_enqueue(ls->q, r) < 0) { free(r->k); free(r->v); free(r); return -1; }
	return 0;
}

int po_logstore_get(po_logstore_t *ls, const void *key, size_t keylen,
					void **out_val, size_t *out_len) {
	// fast path: consult in-memory index under read lock
	uint64_t off = 0;
	uint32_t len = 0;
	pthread_rwlock_rdlock(&ls->idx_lock);
	int have = po_index_get(ls->mem_idx, key, keylen, &off, &len);
	pthread_rwlock_unlock(&ls->idx_lock);
	if (have != 0) {
		// fallback to LMDB
		void *tmp = NULL;
		size_t tmplen = 0;
		int rc = db_get(ls->idx, key, keylen, &tmp, &tmplen);
		if (rc != 0) return -1;
		if (tmplen != 12) { free(tmp); return -1; }
		memcpy(&off, tmp, 8);
		memcpy(&len, (uint8_t *)tmp + 8, 4);
		free(tmp);
		// optionally backfill mem index
		pthread_rwlock_wrlock(&ls->idx_lock);
		(void)po_index_put(ls->mem_idx, key, keylen, off, len);
		pthread_rwlock_unlock(&ls->idx_lock);
	}
	// read from file
	uint32_t kl, vl;
	if (pread(ls->fd, &kl, sizeof(kl), (off_t)off) != (ssize_t)sizeof(kl)) return -1;
	if (pread(ls->fd, &vl, sizeof(vl), (off_t)(off + sizeof(kl))) != (ssize_t)sizeof(vl)) return -1;
	if (kl > 16 * 1024 * 1024 || vl != len) return -1;
	void *buf = malloc(vl);
	if (!buf) return -1;
	off_t val_off = (off_t)(off + sizeof(kl) + sizeof(vl) + (off_t)kl);
	if (pread(ls->fd, buf, vl, val_off) != (ssize_t)vl) { free(buf); return -1; }
	*out_val = buf;
	*out_len = vl;
	return 0;
}

// --- Logger integration ---
// Provide a custom sink that appends formatted lines to the logstore
static void _ls_logger_sink(const char *line, void *ud) {
	po_logstore_t *ls = (po_logstore_t *)ud;
	// Key: 16 bytes [ts_ns(8)][seq(8)] as binary; we don't have raw ts,
	// so we use a monotonic seq and current time.
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	uint64_t ts_ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
	uint64_t seq = atomic_fetch_add(&ls->seq, 1);
	uint8_t key[16];
	memcpy(key, &ts_ns, 8); memcpy(key + 8, &seq, 8);
	size_t vlen = strnlen(line, 4096);
	(void)po_logstore_append(ls, key, sizeof key, line, vlen);
}

int po_logstore_attach_logger(po_logstore_t *ls) {
	// this requires logger to expose custom sink API; if unavailable, return -1
	extern int logger_add_sink_custom(void (*fn)(const char*, void*), void *ud);
	return logger_add_sink_custom(_ls_logger_sink, ls);
}

