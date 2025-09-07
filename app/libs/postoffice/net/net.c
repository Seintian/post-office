/**
 * @file net.c
 * @brief High-level networking API built on protocol and framing.
 */

#include "net/net.h"
#include "metrics/metrics.h"

#include <errno.h>

#include "net/framing.h"
#include "net/protocol.h"
#include "perf/zerocopy.h"

// Simple process-wide TX/RX zero-copy pools for high throughput paths.
// Tunables can be exposed later; defaults chosen for general workloads.
static perf_zcpool_t *g_tx_pool;
static perf_zcpool_t *g_rx_pool;

int net_init_zerocopy(size_t tx_buffers, size_t rx_buffers, size_t buf_size) {
    if (!g_tx_pool) {
        g_tx_pool = perf_zcpool_create(tx_buffers, buf_size);
        if (!g_tx_pool) {
            PO_METRIC_COUNTER_INC("net.zcpool.tx.create.fail");
            return -1;
        }
        PO_METRIC_COUNTER_INC("net.zcpool.tx.create");
        PO_METRIC_COUNTER_ADD("net.zcpool.tx.buffers", (uint64_t)tx_buffers);
    }
    if (!g_rx_pool) {
        g_rx_pool = perf_zcpool_create(rx_buffers, buf_size);
        if (!g_rx_pool) {
            PO_METRIC_COUNTER_INC("net.zcpool.rx.create.fail");
            return -1;
        }
        PO_METRIC_COUNTER_INC("net.zcpool.rx.create");
        PO_METRIC_COUNTER_ADD("net.zcpool.rx.buffers", (uint64_t)rx_buffers);
    }
    return 0;
}

void *net_zcp_acquire_tx(void) {
    if (!g_tx_pool)
        return NULL;
    void *p = perf_zcpool_acquire(g_tx_pool);
    if (p)
        PO_METRIC_COUNTER_INC("net.tx.acquire");
    else
        PO_METRIC_COUNTER_INC("net.tx.acquire.fail");
    return p;
}

void net_zcp_release_tx(void *buf) {
    if (g_tx_pool && buf) {
        perf_zcpool_release(g_tx_pool, buf);
        PO_METRIC_COUNTER_INC("net.tx.release");
    }
}

void *net_zcp_acquire_rx(void) {
    if (!g_rx_pool)
        return NULL;
    void *p = perf_zcpool_acquire(g_rx_pool);
    if (p)
        PO_METRIC_COUNTER_INC("net.rx.acquire");
    else
        PO_METRIC_COUNTER_INC("net.rx.acquire.fail");
    return p;
}

void net_zcp_release_rx(void *buf) {
    if (g_rx_pool && buf) {
        perf_zcpool_release(g_rx_pool, buf);
        PO_METRIC_COUNTER_INC("net.rx.release");
    }
}

int net_send_message(
    int fd,
    uint8_t msg_type,
    uint8_t flags,
    const uint8_t *payload,
    uint32_t payload_len
) {
    PO_METRIC_COUNTER_INC("net.send");
    PO_METRIC_COUNTER_ADD("net.send.bytes", (uint64_t)payload_len);
    po_header_t hdr;
    protocol_init_header(&hdr, msg_type, flags, payload_len);
    int rc = framing_write_msg(fd, &hdr, payload, payload_len);
    if (rc != 0)
        PO_METRIC_COUNTER_INC("net.send.fail");
    return rc;
}

int net_send_message_zcp(
    int fd,
    uint8_t msg_type,
    uint8_t flags,
    void *payload_buf,
    uint32_t payload_len
) {
    PO_METRIC_COUNTER_INC("net.send.zcp");
    PO_METRIC_COUNTER_ADD("net.send.zcp.bytes", (uint64_t)payload_len);
    po_header_t hdr;
    protocol_init_header(&hdr, msg_type, flags, payload_len);
    int rc = framing_write_zcp(fd, &hdr, (const zcp_buffer_t *)payload_buf);
    if (rc != 0)
        PO_METRIC_COUNTER_INC("net.send.zcp.fail");
    return rc;
}

int net_recv_message(int fd, po_header_t *header_out, zcp_buffer_t **payload_out) {
    int rc = framing_read_msg(fd, header_out, payload_out);
    if (rc == 0) {
        PO_METRIC_COUNTER_INC("net.recv");
        PO_METRIC_COUNTER_ADD("net.recv.bytes", (uint64_t)header_out->payload_len);
    } else if (rc != -2) {
        PO_METRIC_COUNTER_INC("net.recv.fail");
    }
    return rc;
}

int net_recv_message_zcp(
    int fd,
    po_header_t *header_out,
    void **payload_out,
    uint32_t *payload_len_out
) {
    void *buf = NULL;
    uint32_t buf_cap = 0;
    if (g_rx_pool) {
        buf = perf_zcpool_acquire(g_rx_pool);
        if (buf)
            buf_cap = (uint32_t)perf_zcpool_bufsize(g_rx_pool);
    }

    if (buf) {
        int rc = framing_read_msg_into(fd, header_out, buf, buf_cap, payload_len_out);
        if (rc == 0) {
            PO_METRIC_COUNTER_INC("net.recv.zcp");
            PO_METRIC_COUNTER_ADD("net.recv.zcp.bytes", (uint64_t)*payload_len_out);
            *payload_out = buf;
            return 0;
        }
        perf_zcpool_release(g_rx_pool, buf);
        PO_METRIC_COUNTER_INC("net.recv.zcp.fail");
    }

    zcp_buffer_t *zbuf = NULL;
    int rc = framing_read_msg(fd, header_out, &zbuf);
    if (rc == 0) {
        PO_METRIC_COUNTER_INC("net.recv");
        PO_METRIC_COUNTER_ADD("net.recv.bytes", (uint64_t)header_out->payload_len);
        *payload_len_out = header_out->payload_len;
        *payload_out = NULL;
    } else if (rc != -2) {
        PO_METRIC_COUNTER_INC("net.recv.fail");
    }

    return rc;
}
