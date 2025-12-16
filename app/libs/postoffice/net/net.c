#include "net/net.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>

#include "metrics/metrics.h"
#include "net/framing.h"
#include "net/protocol.h"
#include "perf/zerocopy.h"

// Simple process-wide TX/RX zero-copy pools for high throughput paths.
// Tunables can be exposed later; defaults chosen for general workloads.
static perf_zcpool_t *g_tx_pool;
static perf_zcpool_t *g_rx_pool;

/* Lightweight synchronization for pool lifecycle:
 * - A small mutex protects concurrent init/shutdown operations.
 * - Atomic "users" counters track active buffer users; shutdown sets
 *   a shutting flag and waits for the counters to reach zero before
 *   destroying the pool. This prevents destroying a pool while a
 *   caller still holds an acquired buffer, and does not require
 *   changing the public API. */
static pthread_mutex_t g_zcpool_create_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_uint g_tx_users = 0;
static atomic_uint g_rx_users = 0;
static atomic_bool g_tx_shutting = false;
static atomic_bool g_rx_shutting = false;

int net_init_zerocopy(size_t tx_buffers, size_t rx_buffers, size_t buf_size) {
    /* Protect init/shutdown races with a mutex. Simple and sufficient for
     * pool lifecycle operations which are infrequent compared to acquire/release. */
    pthread_mutex_lock(&g_zcpool_create_lock);

    if (!g_tx_pool) {
        g_tx_pool = perf_zcpool_create(tx_buffers, buf_size, PERF_ZCPOOL_METRICS);
        if (!g_tx_pool) {
            PO_METRIC_COUNTER_INC("net.zcpool.tx.create.fail");
            pthread_mutex_unlock(&g_zcpool_create_lock);
            return -1;
        }

        PO_METRIC_COUNTER_INC("net.zcpool.tx.create");
        PO_METRIC_COUNTER_ADD("net.zcpool.tx.buffers", tx_buffers);
    }

    if (!g_rx_pool) {
        g_rx_pool = perf_zcpool_create(rx_buffers, buf_size, PERF_ZCPOOL_METRICS);
        if (!g_rx_pool) {
            PO_METRIC_COUNTER_INC("net.zcpool.rx.create.fail");
            /* If TX created above, keep it; caller can shutdown later. */
            pthread_mutex_unlock(&g_zcpool_create_lock);
            return -1;
        }

        PO_METRIC_COUNTER_INC("net.zcpool.rx.create");
        PO_METRIC_COUNTER_ADD("net.zcpool.rx.buffers", rx_buffers);
    }

    pthread_mutex_unlock(&g_zcpool_create_lock);

    return 0;
}

void net_shutdown_zerocopy(void) {
    /* Prevent new acquisitions, wait for active users to drain, then
     * safely destroy the pools. */
    pthread_mutex_lock(&g_zcpool_create_lock);

    atomic_store(&g_tx_shutting, true);
    atomic_store(&g_rx_shutting, true);

    /* Wait for active users to finish. */
    while (atomic_load(&g_tx_users) != 0 || atomic_load(&g_rx_users) != 0) {
        sched_yield();
    }

    if (g_tx_pool) {
        perf_zcpool_destroy(&g_tx_pool);
        g_tx_pool = NULL;
    }
    if (g_rx_pool) {
        perf_zcpool_destroy(&g_rx_pool);
        g_rx_pool = NULL;
    }

    atomic_store(&g_tx_shutting, false);
    atomic_store(&g_rx_shutting, false);

    pthread_mutex_unlock(&g_zcpool_create_lock);
}

void *net_zcp_acquire_tx(void) {
    /* Prevent new acquisitions during shutdown and count active users so
     * shutdown can wait for them. */
    if (atomic_load(&g_tx_shutting))
        return NULL;

    atomic_fetch_add(&g_tx_users, 1);

    /* Re-check shutting flag to avoid a race where shutdown started just
     * after we incremented users. */
    if (atomic_load(&g_tx_shutting)) {
        atomic_fetch_sub(&g_tx_users, 1);
        return NULL;
    }

    if (!g_tx_pool) {
        atomic_fetch_sub(&g_tx_users, 1);
        return NULL;
    }

    void *p = perf_zcpool_acquire(g_tx_pool);
    if (p)
        PO_METRIC_COUNTER_INC("net.tx.acquire");
    else
        PO_METRIC_COUNTER_INC("net.tx.acquire.fail");

    if (!p) /* failed to acquire; decrement user count */
        atomic_fetch_sub(&g_tx_users, 1);

    return p;
}

void net_zcp_release_tx(void *buf) {
    if (!buf) return;

    if (g_tx_pool) {
        perf_zcpool_release(g_tx_pool, buf);
        PO_METRIC_COUNTER_INC("net.tx.release");
    }

    atomic_fetch_sub(&g_tx_users, 1);
}

void *net_zcp_acquire_rx(void) {
    if (atomic_load(&g_rx_shutting))
        return NULL;

    atomic_fetch_add(&g_rx_users, 1);

    if (atomic_load(&g_rx_shutting)) {
        atomic_fetch_sub(&g_rx_users, 1);
        return NULL;
    }

    if (!g_rx_pool) {
        atomic_fetch_sub(&g_rx_users, 1);
        return NULL;
    }

    void *p = perf_zcpool_acquire(g_rx_pool);
    if (p)
        PO_METRIC_COUNTER_INC("net.rx.acquire");
    else
        PO_METRIC_COUNTER_INC("net.rx.acquire.fail");

    if (!p)
        atomic_fetch_sub(&g_rx_users, 1);

    return p;
}

void net_zcp_release_rx(void *buf) {
    if (!buf) return;

    if (g_rx_pool) {
        perf_zcpool_release(g_rx_pool, buf);
        PO_METRIC_COUNTER_INC("net.rx.release");
    }

    atomic_fetch_sub(&g_rx_users, 1);
}

int net_send_message(int fd, uint8_t msg_type, uint8_t flags, const uint8_t *payload,
                     uint32_t payload_len) {
    PO_METRIC_COUNTER_INC("net.send");
    PO_METRIC_COUNTER_ADD("net.send.bytes", payload_len);

    po_header_t hdr;
    protocol_init_header(&hdr, msg_type, flags, payload_len);

    int rc = framing_write_msg(fd, &hdr, payload, payload_len);
    if (rc != 0)
        PO_METRIC_COUNTER_INC("net.send.fail");

    return rc;
}

int net_send_message_zcp(int fd, uint8_t msg_type, uint8_t flags, void *payload_buf,
                         uint32_t payload_len) {
    PO_METRIC_COUNTER_INC("net.send.zcp");
    PO_METRIC_COUNTER_ADD("net.send.zcp.bytes", payload_len);

    po_header_t hdr;
    protocol_init_header(&hdr, msg_type, flags, payload_len);

    int rc = framing_write_zcp(fd, &hdr, (const zcp_buffer_t *)payload_buf);
    if (rc != 0)
        PO_METRIC_COUNTER_INC("net.send.zcp.fail");

    return rc;
}

int net_recv_message(int fd, po_header_t *header_out, zcp_buffer_t **payload_out) {
    void *buf = net_zcp_acquire_rx();
    if (!buf) {
        PO_METRIC_COUNTER_INC("net.recv.acquire.fail");
        errno = ENOMEM;
        return -1;
    }
    /* net_zcp_acquire_rx incremented the active-user counter and validated
     * that g_rx_pool was set at acquire time, and shutdown won't destroy the
     * pool until all users finish. */
    uint32_t buf_cap = (uint32_t)perf_zcpool_bufsize(g_rx_pool);
    uint32_t payload_len = 0;
    int rc = framing_read_msg_into(fd, header_out, buf, buf_cap, &payload_len);
    if (rc == 0) {
        PO_METRIC_COUNTER_INC("net.recv");
        PO_METRIC_COUNTER_ADD("net.recv.bytes", header_out->payload_len);
        *payload_out = (zcp_buffer_t *)buf;
        return 0;
    }

    net_zcp_release_rx(buf);
    if (rc != -2)
        PO_METRIC_COUNTER_INC("net.recv.fail");

    return rc;
}

int net_recv_message_zcp(int fd, po_header_t *header_out, void **payload_out,
                         uint32_t *payload_len_out) {
    void *buf = net_zcp_acquire_rx();
    if (!buf) {
        PO_METRIC_COUNTER_INC("net.recv.zcp.acquire.fail");
        errno = ENOMEM;
        return -1;
    }

    uint32_t buf_cap = (uint32_t)perf_zcpool_bufsize(g_rx_pool);
    int rc = framing_read_msg_into(fd, header_out, buf, buf_cap, payload_len_out);
    if (rc == 0) {
        PO_METRIC_COUNTER_INC("net.recv.zcp");
        PO_METRIC_COUNTER_ADD("net.recv.zcp.bytes", *payload_len_out);

        *payload_out = buf;
        return 0;
    }

    net_zcp_release_rx(buf);
    PO_METRIC_COUNTER_INC("net.recv.zcp.fail");

    return rc;
}
