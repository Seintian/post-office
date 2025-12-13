/**
 * @file framing.c
 * @brief Length-prefixed framing implementation with zero-copy support.
 */



#include "net/framing.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "metrics/metrics.h" // metrics
#include "perf/zerocopy.h"

// Configurable maximum payload length
static uint32_t g_max_payload = FRAMING_DEFAULT_MAX_PAYLOAD;

int framing_init(uint32_t max_payload_bytes) {
    if (max_payload_bytes == 0) {
        g_max_payload = FRAMING_DEFAULT_MAX_PAYLOAD;
        return 0;
    }

    // hard cap at 64 MiB
    const uint32_t cap = 64u * 1024u * 1024u;
    if (max_payload_bytes > cap) {
        errno = EINVAL;
        return -1;
    }

    g_max_payload = max_payload_bytes;
    return 0;
}

uint32_t framing_get_max_payload(void) {
    return g_max_payload;
}

static int write_full(int fd, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    size_t nleft = len;
    while (nleft > 0) {
        ssize_t n = write(fd, p, nleft);
        if (n > 0) {
            p += n;
            nleft -= (size_t)n;
            continue;
        }

        if (n == 0)
            return -2; // peer closed

        if (errno == EINTR)
            continue;

        // Other errors propagate; EAGAIN/EWOULDBLOCK handled by caller as generic error
        return -1;
    }
    return 0;
}

int framing_write_msg(int fd, const po_header_t *header_net, const uint8_t *payload,
                      uint32_t payload_len) {
    if (payload_len > g_max_payload) {
        PO_METRIC_COUNTER_INC("framing.write.size.invalid");
        errno = EMSGSIZE;
        return -1;
    }
    PO_METRIC_COUNTER_INC("framing.write.msg");
    PO_METRIC_COUNTER_ADD("framing.write.msg.bytes", payload_len);

    uint32_t total = (uint32_t)sizeof(po_header_t) + payload_len;
    uint32_t len_be = htonl(total);

    struct iovec iov[3];
    int iovcnt = 0;
    iov[iovcnt].iov_base = &len_be;
    iov[iovcnt].iov_len = sizeof(len_be);
    iovcnt++;

    // cast away const for writev API; buffers aren't modified by kernel
    iov[iovcnt].iov_base = (void *)(uintptr_t)header_net;
    iov[iovcnt].iov_len = sizeof(po_header_t);
    iovcnt++;
    if (payload_len && payload) {
        iov[iovcnt].iov_base = (void *)(uintptr_t)payload;
        iov[iovcnt].iov_len = payload_len;
        iovcnt++;
    }

    // Try writev once; if partial, fallback to write_full on a temp linear buffer
    ssize_t n = writev(fd, iov, iovcnt);
    if (n < 0) {
        if (errno == EINTR)
            return framing_write_msg(fd, header_net, payload, payload_len);
        PO_METRIC_COUNTER_INC("framing.write.msg.fail");
        return -1;
    }

    size_t expected = sizeof(len_be) + sizeof(po_header_t) + payload_len;
    if ((size_t)n == expected)
        return 0;

    // need to finish remaining bytes
    unsigned char tmp_hdr[sizeof(uint32_t) + sizeof(po_header_t)];
    memcpy(tmp_hdr, &len_be, sizeof(uint32_t));
    memcpy(tmp_hdr + sizeof(uint32_t), header_net, sizeof(po_header_t));

    size_t sent = (size_t)n;
    if (sent < sizeof(tmp_hdr)) {
        int rc = write_full(fd, tmp_hdr + sent, sizeof(tmp_hdr) - sent);
        if (rc != 0)
            return rc;

        sent = sizeof(tmp_hdr);
    }

    // remaining payload
    size_t hdr_and_len = sizeof(tmp_hdr);
    if (expected > hdr_and_len) {
        size_t payload_sent = sent > hdr_and_len ? sent - hdr_and_len : 0;
        if (payload_len > payload_sent) {
            const uint8_t *pp = payload + payload_sent;
            return write_full(fd, pp, payload_len - payload_sent);
        }
    }

    return 0;
}

int framing_write_zcp(int fd, const po_header_t *header_net, const zcp_buffer_t *payload_buf) {
    // Use the header's payload_len as authoritative size for the payload.
    // Treat payload_buf as a pointer to contiguous bytes.
    uint32_t payload_len = ntohl(header_net->payload_len);

    if (payload_len > g_max_payload) {
        PO_METRIC_COUNTER_INC("framing.write_zcp.size.invalid");
        errno = EMSGSIZE;
        return -1;
    }
    PO_METRIC_COUNTER_INC("framing.write_zcp");
    PO_METRIC_COUNTER_ADD("framing.write_zcp.bytes", payload_len);

    uint32_t total = (uint32_t)sizeof(po_header_t) + payload_len;
    uint32_t len_be = htonl(total);

    struct iovec iov[3];
    int iovcnt = 0;
    iov[iovcnt].iov_base = &len_be;
    iov[iovcnt].iov_len = sizeof(len_be);
    iovcnt++;

    iov[iovcnt].iov_base = (void *)(uintptr_t)header_net;
    iov[iovcnt].iov_len = sizeof(po_header_t);
    iovcnt++;

    if (payload_len > 0) {
        iov[iovcnt].iov_base = (void *)(uintptr_t)payload_buf;
        iov[iovcnt].iov_len = payload_len;
        iovcnt++;
    }

    ssize_t n = writev(fd, iov, iovcnt);
    if (n < 0) {
        if (errno == EINTR)
            return framing_write_zcp(fd, header_net, payload_buf);
        PO_METRIC_COUNTER_INC("framing.write_zcp.fail");
        return -1;
    }

    size_t expected = sizeof(len_be) + sizeof(po_header_t) + payload_len;
    if ((size_t)n == expected)
        return 0;

    // Finish remaining bytes with write_full fallback
    unsigned char tmp_hdr[sizeof(uint32_t) + sizeof(po_header_t)];
    memcpy(tmp_hdr, &len_be, sizeof(uint32_t));
    memcpy(tmp_hdr + sizeof(uint32_t), header_net, sizeof(po_header_t));

    size_t sent = (size_t)n;
    if (sent < sizeof(tmp_hdr)) {
        int rc = write_full(fd, tmp_hdr + sent, sizeof(tmp_hdr) - sent);
        if (rc != 0)
            return rc;

        sent = sizeof(tmp_hdr);
    }

    size_t hdr_and_len = sizeof(tmp_hdr);
    if (expected > hdr_and_len && payload_len > 0) {
        size_t payload_sent = sent > hdr_and_len ? sent - hdr_and_len : 0;
        if (payload_len > payload_sent) {
            const uint8_t *pp = (const uint8_t *)payload_buf + payload_sent;
            return write_full(fd, pp, payload_len - payload_sent);
        }
    }

    return 0;
}

static int read_full(int fd, void *buf, size_t len) {
    unsigned char *p = (unsigned char *)buf;
    size_t nleft = len;
    while (nleft > 0) {
        ssize_t n = read(fd, p, nleft);
        if (n > 0) {
            p += n;
            nleft -= (size_t)n;
            continue;
        }

        if (n == 0)
            return -2; // EOF

        if (errno == EINTR)
            continue;

        // Report all errors uniformly (including EAGAIN/EWOULDBLOCK)
        return -1;
    }

    return 0;
}

int framing_read_msg_into(int fd, po_header_t *header_out, void *payload_buf,
                          uint32_t payload_buf_size, uint32_t *payload_len_out) {
    // 1. Peek at the length prefix to check if we have a full message
    uint32_t len_be = 0;
    ssize_t pn = recv(fd, &len_be, sizeof(len_be), MSG_PEEK | MSG_DONTWAIT);
    if (pn < (ssize_t)sizeof(len_be)) {
        if (pn == 0) return -2; // EOF
        // If error (including EAGAIN) or partial peek, return EAGAIN to avoid desync
        if (pn < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            PO_METRIC_COUNTER_INC("framing.read_into.len.peek_fail");
            return -1;
        }
        errno = EAGAIN;
        return -1;
    }

    // 2. Check if total message bytes are available
    uint32_t peeked_total = ntohl(len_be);

    // Early validation of the peeked length
    if (peeked_total < sizeof(po_header_t)) {
        errno = EPROTO;
        return -1;
    }
    if (peeked_total - sizeof(po_header_t) > g_max_payload) {
        PO_METRIC_COUNTER_INC("framing.read.size.invalid");
        errno = EMSGSIZE;
        return -1;
    }

    int avail = 0;
#ifdef FIONREAD
    if (ioctl(fd, FIONREAD, &avail) < 0) {
        // Fallback: assume insufficient data and rely on blocking read semantics
        avail = 0;
    }
#endif
    // If we got a valid avail count, check if we have enough bytes
    if (avail > 0 && (size_t)avail < sizeof(len_be) + peeked_total) {
        errno = EAGAIN;
        return -1;
    }

    // 3. Read for real (guaranteed not to block if FIONREAD was correct)
    uint32_t len_be_actual = 0;
    int rc = read_full(fd, &len_be_actual, sizeof(len_be_actual));
    if (rc != 0) {
        if (rc == -1) PO_METRIC_COUNTER_INC("framing.read_into.len.fail");
        return rc;
    }
    uint32_t total = ntohl(len_be_actual);
    if (total < sizeof(po_header_t)) {
        errno = EPROTO;
        return -1;
    }

    po_header_t net_hdr;
    rc = read_full(fd, &net_hdr, sizeof(net_hdr));
    if (rc != 0) {
        if (rc == -1) PO_METRIC_COUNTER_INC("framing.read_into.hdr.fail");
        return rc;
    }

    *header_out = net_hdr;
    protocol_header_to_host(header_out);
    if (header_out->version != PROTOCOL_VERSION) {
        errno = EPROTONOSUPPORT;
        return -1;
    }

    uint32_t payload_len = total - (uint32_t)sizeof(po_header_t);
    if (payload_len > g_max_payload) {
        PO_METRIC_COUNTER_INC("framing.read.size.invalid");
        errno = EMSGSIZE;
        return -1;
    }
    if (payload_len == 0) {
        if (payload_len_out)
            *payload_len_out = 0;
        PO_METRIC_COUNTER_INC("framing.read_into.msg");
        return 0;
    }
    if (!payload_buf || payload_buf_size < payload_len) {
        PO_METRIC_COUNTER_INC("framing.read.buf.overflow");
        errno = EMSGSIZE;
        return -1;
    }

    rc = read_full(fd, payload_buf, payload_len);
    if (rc != 0) {
        if (rc == -1) PO_METRIC_COUNTER_INC("framing.read_into.payload.fail");
        return rc;
    }

    if (payload_len_out)
        *payload_len_out = payload_len;

    PO_METRIC_COUNTER_INC("framing.read_into.msg");
    PO_METRIC_COUNTER_ADD("framing.read_into.msg.bytes", payload_len);
    return 0;
}
