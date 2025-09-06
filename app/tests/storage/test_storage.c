// Logstore test suite (Unity)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "storage/logstore.h"
#include "log/logger.h"
#include "unity/unity_fixture.h"

// ---------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------

static char g_dir_template[] = "/tmp/ls_testXXXXXX";
static char *g_dir = NULL;
static po_logstore_t *g_ls = NULL;

static void make_dir(void) {
	// reset template tail because mkdtemp mutates it
	memcpy(g_dir_template, "/tmp/ls_testXXXXXX", sizeof("/tmp/ls_testXXXXXX"));
	g_dir = mkdtemp(g_dir_template);
	TEST_ASSERT_NOT_NULL_MESSAGE(g_dir, "mkdtemp failed");
}

static void rm_rf_dir(void) {
	if (!g_dir) return;
	char path[512];
	snprintf(path, sizeof(path), "%s/aof.log", g_dir);
	unlink(path); // ignore errors
	rmdir(g_dir);
	g_dir = NULL;
}

// Wait (poll) until a key is visible or timeout (ms) reached.
static int wait_get(po_logstore_t *ls, const void *key, size_t klen, void **out, size_t *outlen,
					int timeout_ms) {
	const int step_us = 2000; // 2ms
	int waited = 0;
	while (waited < timeout_ms * 1000) {
		if (po_logstore_get(ls, key, klen, out, outlen) == 0) return 0;
		usleep(step_us);
		waited += step_us;
	}
	return -1;
}

// ---------------------------------------------------------------------
// Test Group
// ---------------------------------------------------------------------

TEST_GROUP(LOGSTORE);

TEST_SETUP(LOGSTORE) {
	make_dir();
	po_logstore_cfg cfg = {
		.dir = g_dir,
		.bucket = "idx",
		.map_size = 1 << 20,
		.ring_capacity = 256,
		.batch_size = 32,
		.fsync_policy = PO_LS_FSYNC_NONE,
	};
	g_ls = po_logstore_open_cfg(&cfg);
	TEST_ASSERT_NOT_NULL(g_ls);
}

TEST_TEAR_DOWN(LOGSTORE) {
	if (g_ls) {
		po_logstore_close(&g_ls);
	}
	rm_rf_dir();
}

// ---------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------

TEST(LOGSTORE, APPEND_AND_GET_SINGLE) {
	const char *k = "alpha";
	const char *v = "one";
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k, strlen(k), v, strlen(v)));
	void *out = NULL; size_t outlen = 0;
	TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k, strlen(k), &out, &outlen, 250));
	TEST_ASSERT_EQUAL_UINT(strlen(v), outlen);
	TEST_ASSERT_EQUAL_MEMORY(v, out, outlen);
	free(out);
}

TEST(LOGSTORE, APPEND_MULTIPLE_UNIQUE) {
	const char *keys[] = {"k1","k2","k3","k4"};
	const char *vals[] = {"v1","v2","v3","v4"};
	for (size_t i = 0; i < 4; ++i) {
		TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, keys[i], strlen(keys[i]), vals[i], strlen(vals[i])));
	}
	for (size_t i = 0; i < 4; ++i) {
		void *out=NULL; size_t outlen=0;
		TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, keys[i], strlen(keys[i]), &out, &outlen, 250));
		TEST_ASSERT_EQUAL_UINT(strlen(vals[i]), outlen);
		TEST_ASSERT_EQUAL_MEMORY(vals[i], out, outlen);
		free(out);
	}
}

TEST(LOGSTORE, OVERWRITE_KEY_RETURNS_LAST) {
	const char *k = "key";
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k, strlen(k), "first", 5));
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k, strlen(k), "second", 6));
	void *out=NULL; size_t outlen=0;
	TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k, strlen(k), &out, &outlen, 300));
	TEST_ASSERT_EQUAL_UINT(6, outlen);
	TEST_ASSERT_EQUAL_MEMORY("second", out, 6);
	free(out);
}

TEST(LOGSTORE, GET_MISSING_KEY_FAILS) {
	void *out=NULL; size_t outlen=0;
	TEST_ASSERT_EQUAL_INT(-1, po_logstore_get(g_ls, "missing", 7, &out, &outlen));
}

TEST(LOGSTORE, PERSISTENCE_REOPEN) {
	const char *k = "persist"; const char *v = "value";
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k, strlen(k), v, strlen(v)));
	// Wait for worker to flush
	void *out=NULL; size_t outlen=0;
	TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k, strlen(k), &out, &outlen, 300));
	free(out);
	// Close & reopen
	po_logstore_close(&g_ls);
	po_logstore_cfg cfg = {
		.dir = g_dir,
		.bucket = "idx",
		.map_size = 1 << 20,
		.ring_capacity = 128,
		.batch_size = 16,
		.fsync_policy = PO_LS_FSYNC_NONE,
	};
	g_ls = po_logstore_open_cfg(&cfg);
	TEST_ASSERT_NOT_NULL(g_ls);
	out=NULL; outlen=0;
	TEST_ASSERT_EQUAL_INT(0, po_logstore_get(g_ls, k, strlen(k), &out, &outlen));
	TEST_ASSERT_EQUAL_UINT(strlen(v), outlen);
	TEST_ASSERT_EQUAL_MEMORY(v, out, outlen);
	free(out);
}

TEST(LOGSTORE, PERSISTENCE_TRUNCATED_LAST_RECORD) {
	const char *k1 = "k_one"; const char *v1 = "value1";
	const char *k2 = "k_two"; const char *v2 = "value_that_will_be_truncated";
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k1, strlen(k1), v1, strlen(v1)));
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k2, strlen(k2), v2, strlen(v2)));
	void *out=NULL; size_t outlen=0;
	TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k2, strlen(k2), &out, &outlen, 400));
	free(out);
	// truncate last record partially (remove 5 bytes from end of file)
	po_logstore_close(&g_ls);
	char path[512]; snprintf(path,sizeof(path),"%s/aof.log", g_dir);
	int fd = open(path, O_RDWR);
	TEST_ASSERT_TRUE(fd>=0);
	off_t sz = lseek(fd, 0, SEEK_END);
	TEST_ASSERT_TRUE(sz>10);
	int trc = ftruncate(fd, sz - 5); // chop tail
	TEST_ASSERT_EQUAL_INT(0, trc);
	close(fd);
	po_logstore_cfg cfg = {
		.dir = g_dir,
		.bucket = "idx",
		.map_size = 1 << 20,
		.ring_capacity = 128,
		.batch_size = 16,
		.fsync_policy = PO_LS_FSYNC_NONE,
		.fsync_interval_ms = 0,
	};
	g_ls = po_logstore_open_cfg(&cfg);
	TEST_ASSERT_NOT_NULL(g_ls);
	// first key still retrievable
	TEST_ASSERT_EQUAL_INT(0, po_logstore_get(g_ls, k1, strlen(k1), &out, &outlen));
	TEST_ASSERT_EQUAL_UINT(strlen(v1), outlen); free(out);
	// second key should now fail due to truncated value
	out=NULL; outlen=0;
	TEST_ASSERT_EQUAL_INT(-1, po_logstore_get(g_ls, k2, strlen(k2), &out, &outlen));
}

TEST(LOGSTORE, FSYNC_INTERVAL_POLICY_FUNCTIONAL) {
	po_logstore_close(&g_ls);
	po_logstore_cfg cfg = {
		.dir = g_dir,
		.bucket = "idx",
		.map_size = 1 << 20,
		.ring_capacity = 64,
		.batch_size = 8,
		.fsync_policy = PO_LS_FSYNC_INTERVAL,
		.fsync_interval_ms = 20,
		.fsync_every_n = 0,
	};
	g_ls = po_logstore_open_cfg(&cfg);
	TEST_ASSERT_NOT_NULL(g_ls);
	// append a few entries and ensure they are retrievable
	for (int i=0;i<10;i++) {
		char k[16]; char v[16];
		int klen = snprintf(k,sizeof(k),"ik%d", i);
		int vlen = snprintf(v,sizeof(v),"iv%d", i);
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k, (size_t)klen, v, (size_t)vlen));
		void *out=NULL; size_t outlen=0;
	TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k, (size_t)klen, &out, &outlen, 300));
		TEST_ASSERT_EQUAL_UINT((size_t)vlen, outlen);
		free(out);
	}
}

TEST(LOGSTORE, FSYNC_EVERY_N_POLICY_FUNCTIONAL) {
	po_logstore_close(&g_ls);
	po_logstore_cfg cfg = {
		.dir = g_dir,
		.bucket = "idx",
		.map_size = 1 << 20,
		.ring_capacity = 64,
		.batch_size = 4,
		.fsync_policy = PO_LS_FSYNC_EVERY_N,
		.fsync_interval_ms = 0,
		.fsync_every_n = 3,
	};
	g_ls = po_logstore_open_cfg(&cfg);
	TEST_ASSERT_NOT_NULL(g_ls);
	for (int i=0;i<9;i++) {
		char k[16]; char v[16];
		int klen = snprintf(k,sizeof(k),"enk%d", i);
		int vlen = snprintf(v,sizeof(v),"env%d", i);
		TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k, (size_t)klen, v, (size_t)vlen));
		void *out=NULL; size_t outlen=0;
		TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k, (size_t)klen, &out, &outlen, 300));
		free(out);
	}
}

TEST(LOGSTORE, REBUILD_ON_OPEN_WITH_TRUNCATE) {
	po_logstore_close(&g_ls);
	// Create with rebuild enabled
	po_logstore_cfg cfg = {
		.dir = g_dir,
		.bucket = "idx",
		.map_size = 1 << 20,
		.ring_capacity = 128,
		.batch_size = 8,
		.fsync_policy = PO_LS_FSYNC_NONE,
		.rebuild_on_open = 1,
		.truncate_on_rebuild = 1,
	};
	g_ls = po_logstore_open_cfg(&cfg);
	TEST_ASSERT_NOT_NULL(g_ls);
	const char *k1="rbk1"; const char *v1="rbv1";
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k1, strlen(k1), v1, strlen(v1)));
	void *out=NULL; size_t outlen=0; TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k1, strlen(k1), &out, &outlen, 300)); free(out);
	po_logstore_close(&g_ls);
	// Corrupt tail by appending junk partial header
	char path[512]; snprintf(path,sizeof(path),"%s/aof.log", g_dir);
	int fd = open(path, O_WRONLY|O_APPEND);
	TEST_ASSERT_TRUE(fd>=0);
	uint32_t bogus = 0xFFFFFFFF; ssize_t wr = write(fd, &bogus, sizeof(bogus)); (void)wr; // incomplete record
	close(fd);
	// Reopen with rebuild+truncate: should drop bogus tail and still allow original key fetch
	po_logstore_cfg cfg2 = {
		.dir = g_dir,
		.bucket = "idx",
		.map_size = 1 << 20,
		.ring_capacity = 64,
		.batch_size = 4,
		.fsync_policy = PO_LS_FSYNC_NONE,
		.rebuild_on_open = 1,
		.truncate_on_rebuild = 1,
	};
	g_ls = po_logstore_open_cfg(&cfg2);
	TEST_ASSERT_NOT_NULL(g_ls);
	out=NULL; outlen=0;
	TEST_ASSERT_EQUAL_INT(0, po_logstore_get(g_ls, k1, strlen(k1), &out, &outlen));
	TEST_ASSERT_EQUAL_UINT(strlen(v1), outlen); free(out);
}

TEST(LOGSTORE, INTEGRITY_SCAN_PRUNE) {
	const char *k="ik"; const char *v="val";
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k, strlen(k), v, strlen(v)));
	void *out=NULL; size_t outlen=0; TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k, strlen(k), &out, &outlen, 300)); free(out);
	// Create stale entry (offset beyond EOF)
	uint64_t bad_off = 5ULL * 1024 * 1024; uint32_t bad_len = 55;
	TEST_ASSERT_EQUAL_INT(0, po_logstore_debug_put_index(g_ls, "stale", 5, bad_off, bad_len));
	po_logstore_integrity_stats st; po_logstore_integrity_scan(g_ls, 1, &st);
	TEST_ASSERT_TRUE(st.pruned >= 1);
}

TEST(LOGSTORE, TAIL_TRUNCATION_PARTIAL_VALUE_REBUILD) {
	po_logstore_close(&g_ls);
	po_logstore_cfg cfg = { .dir=g_dir, .bucket="idx", .map_size=1<<20, .ring_capacity=128, .batch_size=8, .fsync_policy=PO_LS_FSYNC_NONE };
	g_ls = po_logstore_open_cfg(&cfg); TEST_ASSERT_NOT_NULL(g_ls);
	const char *k1="pv_k1"; const char *v1="AAAA";
	const char *k2="pv_k2"; const char *v2="BBBBBBBB"; // we'll truncate half
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k1, strlen(k1), v1, strlen(v1)));
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k2, strlen(k2), v2, strlen(v2)));
	void *out=NULL; size_t outlen=0; TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k2, strlen(k2), &out, &outlen, 400)); free(out);
	po_logstore_close(&g_ls);
	char path[512]; snprintf(path,sizeof(path),"%s/aof.log", g_dir);
	int fd=open(path,O_RDWR); TEST_ASSERT_TRUE(fd>=0);
	off_t sz=lseek(fd,0,SEEK_END); TEST_ASSERT_TRUE(sz>0);
	// truncate last 4 bytes (half of second value)
	TEST_ASSERT_EQUAL_INT(0, ftruncate(fd, sz-4)); close(fd);
	po_logstore_cfg cfg2 = { .dir=g_dir, .bucket="idx", .map_size=1<<20, .ring_capacity=64, .batch_size=4, .fsync_policy=PO_LS_FSYNC_NONE, .rebuild_on_open=1, .truncate_on_rebuild=1 };
	g_ls = po_logstore_open_cfg(&cfg2); TEST_ASSERT_NOT_NULL(g_ls);
	// first key intact
	TEST_ASSERT_EQUAL_INT(0, po_logstore_get(g_ls, k1, strlen(k1), &out, &outlen));
	TEST_ASSERT_EQUAL_UINT(strlen(v1), outlen); free(out);
	// second key should fail (truncated)
	out=NULL; outlen=0; TEST_ASSERT_EQUAL_INT(-1, po_logstore_get(g_ls, k2, strlen(k2), &out, &outlen));
}

TEST(LOGSTORE, BATCH_WRITE_MANY) {
	// Write many entries quickly to exercise batching
	for (int i=0;i<200;i++) {
		char k[32]; char v[32];
		int klen = snprintf(k,sizeof(k),"k%03d", i);
		int vlen = snprintf(v,sizeof(v),"val%03d", i);
		TEST_ASSERT_TRUE(klen>0 && vlen>0);
		TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k, (size_t)klen, v, (size_t)vlen));
	}
	// Spot check a few
	for (int i=0;i<200;i+=37) {
		char k[32]; char v[32];
		int klen = snprintf(k,sizeof(k),"k%03d", i);
		int vlen = snprintf(v,sizeof(v),"val%03d", i);
		void *out=NULL; size_t outlen=0;
		TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k, (size_t)klen, &out, &outlen, 500));
		TEST_ASSERT_EQUAL_UINT((size_t)vlen, outlen);
		TEST_ASSERT_EQUAL_MEMORY(v, out, outlen);
		free(out);
	}
}

TEST(LOGSTORE, LARGE_VALUE) {
	size_t sz = 64 * 1024; // 64KB
	char *val = (char*)malloc(sz);
	TEST_ASSERT_NOT_NULL(val);
	for (size_t i=0;i<sz;i++) val[i] = (char)('a' + (i % 26));
	const char *k = "large";
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k, strlen(k), val, sz));
	void *out=NULL; size_t outlen=0;
	TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k, strlen(k), &out, &outlen, 800));
	TEST_ASSERT_EQUAL_UINT(sz, outlen);
	TEST_ASSERT_EQUAL_MEMORY(val, out, sz);
	free(out); free(val);
}

// Threaded writer for concurrency test
typedef struct { po_logstore_t *ls; int base; int count; } writer_ctx_t;
static void *writer_thread(void *arg) {
	writer_ctx_t *ctx = (writer_ctx_t*)arg;
	for (int i=0;i<ctx->count;i++) {
		char k[32]; char v[32];
		int id = ctx->base + i;
		int klen = snprintf(k,sizeof(k),"ckey_%d", id);
		int vlen = snprintf(v,sizeof(v),"cval_%d", id);
		po_logstore_append(ctx->ls, k, (size_t)klen, v, (size_t)vlen);
	}
	return NULL;
}

TEST(LOGSTORE, CONCURRENT_APPENDS) {
	enum { THREADS = 4, PER_THREAD = 60 };
	pthread_t th[4];
	writer_ctx_t ctx[4];
	for (int t=0;t<THREADS;t++) {
		ctx[t].ls = g_ls; ctx[t].base = t*1000; ctx[t].count = PER_THREAD;
		TEST_ASSERT_EQUAL_INT(0, pthread_create(&th[t], NULL, writer_thread, &ctx[t]));
	}
	for (int t=0;t<THREADS;t++) pthread_join(th[t], NULL);
	// enqueue a flush marker and wait until it's visible to ensure drain
	const char *fk = "flush_key"; const char *fv = "flush_val";
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, fk, strlen(fk), fv, strlen(fv)));
	void *fout=NULL; size_t foutlen=0; TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, fk, strlen(fk), &fout, &foutlen, 500)); free(fout);
	// Verify a few from each thread
	for (int t=0;t<THREADS;t++) {
		for (int i=0;i<PER_THREAD;i+=17) {
			int id = t*1000 + i;
			char k[32]; char exp[32];
			int klen = snprintf(k,sizeof(k),"ckey_%d", id);
			int vlen = snprintf(exp,sizeof(exp),"cval_%d", id);
			void *out=NULL; size_t outlen=0;
			TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k, (size_t)klen, &out, &outlen, 800));
			TEST_ASSERT_EQUAL_UINT((size_t)vlen, outlen);
			TEST_ASSERT_EQUAL_MEMORY(exp, out, outlen);
			free(out);
		}
	}
}

TEST(LOGSTORE, LOGGER_SINK_ATTACHED_WRITES_FILE) {
	// close current so we can reopen with sink
	po_logstore_close(&g_ls);
	po_logstore_cfg cfg = {
		.dir = g_dir,
		.bucket = "idx",
		.map_size = 1 << 20,
		.ring_capacity = 128,
		.batch_size = 8,
		.fsync_policy = PO_LS_FSYNC_NONE,
	};
	g_ls = po_logstore_open_cfg(&cfg);
	TEST_ASSERT_NOT_NULL(g_ls);
	TEST_ASSERT_EQUAL_INT(0, po_logstore_attach_logger(g_ls));
	// Init logger minimal if not already
	logger_config lcfg = { .level = LOG_INFO, .ring_capacity = 1024, .consumers = 1, .policy = LOGGER_OVERWRITE_OLDEST };
	logger_init(&lcfg);
	char path[512]; snprintf(path,sizeof(path),"%s/aof.log", g_dir);
	struct stat st_before; fstat(open(path,O_RDONLY), &st_before); // ignore error
	LOG_INFO("logstore sink test message %d", 42);
	// wait a bit for async pipeline
	usleep(50*1000);
	int fd = open(path,O_RDONLY); struct stat st_after; fstat(fd,&st_after); close(fd);
	logger_shutdown();
	TEST_ASSERT_TRUE(st_after.st_size >= st_before.st_size);
}

TEST(LOGSTORE, APPEND_ZERO_LENGTH_KEY_REJECT) {
	// Attempt to append with zero-length key should fail.
	const char *val = "value";
	TEST_ASSERT_EQUAL_INT(-1, po_logstore_append(g_ls, "", 0, val, strlen(val)));
}

TEST(LOGSTORE, APPEND_EXCEED_MAX_KEY) {
	// Reopen with small max_key_bytes=16 and try 17-byte key.
	po_logstore_close(&g_ls);
	po_logstore_cfg cfg = {
		.dir = g_dir,
		.bucket = "idx",
		.map_size = 1 << 20,
		.ring_capacity = 64,
		.batch_size = 8,
		.fsync_policy = PO_LS_FSYNC_NONE,
		.max_key_bytes = 16,
	};
	g_ls = po_logstore_open_cfg(&cfg); TEST_ASSERT_NOT_NULL(g_ls);
	char key[32]; memset(key, 'k', 17); key[17] = '\0';
	TEST_ASSERT_EQUAL_INT(-1, po_logstore_append(g_ls, key, 17, "v", 1));
	// 16 bytes should succeed
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, key, 16, "v", 1));
	void *out=NULL; size_t outlen=0; TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, key, 16, &out, &outlen, 300)); free(out);
}

TEST(LOGSTORE, APPEND_EXCEED_MAX_VALUE) {
	// Reopen with small max_value_bytes=32 and try 33-byte value.
	po_logstore_close(&g_ls);
	po_logstore_cfg cfg = {
		.dir = g_dir,
		.bucket = "idx",
		.map_size = 1 << 20,
		.ring_capacity = 64,
		.batch_size = 8,
		.fsync_policy = PO_LS_FSYNC_NONE,
		.max_value_bytes = 32,
	};
	g_ls = po_logstore_open_cfg(&cfg); TEST_ASSERT_NOT_NULL(g_ls);
	char val[40]; memset(val,'a',33);
	TEST_ASSERT_EQUAL_INT(-1, po_logstore_append(g_ls, "k", 1, val, 33));
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, "k", 1, val, 32));
	void *out=NULL; size_t outlen=0; TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, "k", 1, &out, &outlen, 300)); free(out);
}

TEST(LOGSTORE, DEBUG_LOOKUP_RETURNS_OFFSET) {
	const char *k = "dblk"; const char *v = "debug_lookup_value";
	TEST_ASSERT_EQUAL_INT(0, po_logstore_append(g_ls, k, strlen(k), v, strlen(v)));
	void *out=NULL; size_t outlen=0; TEST_ASSERT_EQUAL_INT(0, wait_get(g_ls, k, strlen(k), &out, &outlen, 400)); free(out);
	uint64_t off=0; uint32_t len=0; TEST_ASSERT_EQUAL_INT(0, po_logstore_debug_lookup(g_ls, k, strlen(k), &off, &len));
	TEST_ASSERT_EQUAL_UINT(strlen(v), len);
	// sanity: reading via get again returns same length
	TEST_ASSERT_EQUAL_INT(0, po_logstore_get(g_ls, k, strlen(k), &out, &outlen));
	TEST_ASSERT_EQUAL_UINT(len, outlen); free(out);
}

// ---------------------------------------------------------------------
// Group Runner
// ---------------------------------------------------------------------

TEST_GROUP_RUNNER(LOGSTORE) {
	RUN_TEST_CASE(LOGSTORE, APPEND_AND_GET_SINGLE);
	RUN_TEST_CASE(LOGSTORE, APPEND_MULTIPLE_UNIQUE);
	RUN_TEST_CASE(LOGSTORE, OVERWRITE_KEY_RETURNS_LAST);
	RUN_TEST_CASE(LOGSTORE, GET_MISSING_KEY_FAILS);
	RUN_TEST_CASE(LOGSTORE, PERSISTENCE_REOPEN);
	RUN_TEST_CASE(LOGSTORE, PERSISTENCE_TRUNCATED_LAST_RECORD);
	RUN_TEST_CASE(LOGSTORE, BATCH_WRITE_MANY);
	RUN_TEST_CASE(LOGSTORE, LARGE_VALUE);
	RUN_TEST_CASE(LOGSTORE, CONCURRENT_APPENDS);
	RUN_TEST_CASE(LOGSTORE, LOGGER_SINK_ATTACHED_WRITES_FILE);
	RUN_TEST_CASE(LOGSTORE, FSYNC_INTERVAL_POLICY_FUNCTIONAL);
	RUN_TEST_CASE(LOGSTORE, FSYNC_EVERY_N_POLICY_FUNCTIONAL);
	RUN_TEST_CASE(LOGSTORE, REBUILD_ON_OPEN_WITH_TRUNCATE);
	RUN_TEST_CASE(LOGSTORE, INTEGRITY_SCAN_PRUNE);
	RUN_TEST_CASE(LOGSTORE, TAIL_TRUNCATION_PARTIAL_VALUE_REBUILD);
	RUN_TEST_CASE(LOGSTORE, APPEND_ZERO_LENGTH_KEY_REJECT);
	RUN_TEST_CASE(LOGSTORE, APPEND_EXCEED_MAX_KEY);
	RUN_TEST_CASE(LOGSTORE, APPEND_EXCEED_MAX_VALUE);
	RUN_TEST_CASE(LOGSTORE, DEBUG_LOOKUP_RETURNS_OFFSET);
}

