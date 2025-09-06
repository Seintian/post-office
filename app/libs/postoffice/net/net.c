/**
 * @file net.c
 * @brief High-level networking API built on protocol and framing.
 */

#include "net/net.h"

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
        if (!g_tx_pool)
            return -1;
    }
    if (!g_rx_pool) {
        g_rx_pool = perf_zcpool_create(rx_buffers, buf_size);
        if (!g_rx_pool)
            return -1;
    }
    return 0;
}

void *net_zcp_acquire_tx(void) {
    if (!g_tx_pool)
        return NULL;
    return perf_zcpool_acquire(g_tx_pool);
}

void net_zcp_release_tx(void *buf) {
    if (g_tx_pool && buf)
        perf_zcpool_release(g_tx_pool, buf);
}

void *net_zcp_acquire_rx(void) {
    if (!g_rx_pool)
        return NULL;
    return perf_zcpool_acquire(g_rx_pool);
}

void net_zcp_release_rx(void *buf) {
    if (g_rx_pool && buf)
        perf_zcpool_release(g_rx_pool, buf);
}

int net_send_message(int fd, uint8_t msg_type, uint8_t flags, const uint8_t *payload,
                     uint32_t payload_len) {
    po_header_t hdr;
    protocol_init_header(&hdr, msg_type, flags, payload_len);

    // hdr is already network order
    return framing_write_msg(fd, &hdr, payload, payload_len);
}

int net_send_message_zcp(int fd, uint8_t msg_type, uint8_t flags, void *payload_buf,
                         uint32_t payload_len) {
    po_header_t hdr;
    protocol_init_header(&hdr, msg_type, flags, payload_len);
    return framing_write_zcp(fd, &hdr, (const zcp_buffer_t *)payload_buf);
}

int net_recv_message(int fd, po_header_t *header_out, zcp_buffer_t **payload_out) {
    // framing_read_msg already converts header to host order
    return framing_read_msg(fd, header_out, payload_out);
}

int net_recv_message_zcp(int fd, po_header_t *header_out, void **payload_out,
                         uint32_t *payload_len_out) {
    // Try to use RX pool and read directly into it.
    void *buf = NULL;
    uint32_t buf_cap = 0;
    if (g_rx_pool) {
        buf = perf_zcpool_acquire(g_rx_pool);
        if (buf) {
            buf_cap = (uint32_t)perf_zcpool_bufsize(g_rx_pool);
        }
    }

    if (buf) {
        int rc = framing_read_msg_into(fd, header_out, buf, buf_cap, payload_len_out);
        if (rc == 0) {
            *payload_out = buf;
            return 0;
        }

        // On failure, release and fall back
        perf_zcpool_release(g_rx_pool, buf);
    }

    // Fallback to classic path (payload discarded currently)
    zcp_buffer_t *zbuf = NULL;
    int rc = framing_read_msg(fd, header_out, &zbuf);
    if (rc != 0)
        return rc;

    *payload_len_out = header_out->payload_len;
    *payload_out = NULL;
    return 0;
}
