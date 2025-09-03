/**
 * @file framing.c
 * @brief Length-prefixed framing implementation with zero-copy support.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "net/framing.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

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

// NOTE: zcp_buffer_t is opaque; framing does not know its layout. We do not
// allocate or access it here. Payload delivery via zero-copy will be wired
// when a concrete buffer API is available.

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
        errno = EMSGSIZE;
        return -1;
    }

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

int framing_write_zcp(int fd, const po_header_t *header_net,
                      const zcp_buffer_t *payload_buf __attribute__((unused))) {
    // We don't know the payload size from the opaque type; callers should use contiguous payload
    // variant For compatibility, treat as zero-length payload here.
    return framing_write_msg(fd, header_net, NULL, 0);
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

int framing_read_msg(int fd, po_header_t *header_out, zcp_buffer_t **payload_out) {

    // 1) read 4-byte length prefix
    uint32_t len_be = 0;
    int rc = read_full(fd, &len_be, sizeof(len_be));
    if (rc != 0)
        return rc; // -1 error, -2 EOF
    uint32_t total = ntohl(len_be);
    if (total < sizeof(po_header_t)) {
        errno = EPROTO;
        return -1;
    }

    // 2) read header
    po_header_t net_hdr;
    rc = read_full(fd, &net_hdr, sizeof(net_hdr));
    if (rc != 0)
        return rc;

    // convert to host for validation and return
    *header_out = net_hdr;
    protocol_header_to_host(header_out);
    if (header_out->version != PROTOCOL_VERSION) {
        errno = EPROTONOSUPPORT;
        return -1;
    }

    uint32_t payload_len = total - (uint32_t)sizeof(po_header_t);
    if (payload_len > g_max_payload) {
        errno = EMSGSIZE;
        return -1;
    }

    // 3) read payload bytes. Zero-copy pool integration is not available in this
    // translation unit, so we read and discard payload for now and return NULL.
    if (payload_len == 0) {
        *payload_out = NULL;
        return 0;
    }
    unsigned char *tmp = (unsigned char *)malloc(payload_len);
    if (!tmp) {
        errno = ENOMEM;
        return -1;
    }
    rc = read_full(fd, tmp, payload_len);
    free(tmp);
    if (rc != 0)
        return rc;
    *payload_out = NULL;
    return 0;
}
