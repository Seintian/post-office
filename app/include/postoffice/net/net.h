#ifndef _PO_NET_H
#define _PO_NET_H

#include <stdint.h>
#include <stddef.h>
#include "utils/errors.h"


#ifdef __cplusplus
extern "C" {
#endif

// High‑level connection handle
typedef struct _po_conn_t po_conn_t;

// Initialize networking (sets up epoll, etc.)
int po_net_init(void);

// Listen on a Unix‑domain or TCP socket
//   path: "unix:/tmp/po.sock"  or "127.0.0.1"
//   port: 0 for Unix‑domain, or TCP port number
po_conn_t *po_listen(const char *path, uint16_t port);

// Connect to a listening endpoint
po_conn_t *po_connect(const char *path, uint16_t port);

// Send a single framed message (hdr + payload)
int po_send(
    po_conn_t *c,
    uint8_t msg_type,
    uint8_t flags,
    const void *payload,
    uint32_t payload_len
);

// Try to receive one full message
//   out_payload is malloc'd, caller must free()
int po_recv(
    po_conn_t *c,
    uint8_t *out_msg_type,
    uint8_t *out_flags,
    void **out_payload,
    uint32_t *out_payload_len
);

// Poll for events on all connections (timeout_ms: -1 = block)
int po_poll(int timeout_ms);

// Close a connection
void po_close(po_conn_t *c);

#ifdef __cplusplus
}
#endif

#endif // _PO_NET_H