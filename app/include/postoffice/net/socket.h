/**
 * @file socket.h
 * @brief TCP and UNIX-domain socket utility functions.
 * @ingroup net
 *
 * This header provides minimal socket helpers used by the PostOffice net
 * implementation. All sockets returned by these helpers are configured with
 * CLOEXEC and set to non-blocking mode by default.
 *
 * Conventions:
 *  - Return -1 for hard errors (inspect errno).
 *  - Return PO_SOCKET_WOULDBLOCK (-2) for transient EAGAIN/EWOULDBLOCK where
 *    the operation should be retried after readiness notification.
 *  - For connect routines, EINPROGRESS is surfaced via errno with a -1 return.
 *
 * @see poller.h for readiness integration.
 */

#ifndef _SOCKET_H
#define _SOCKET_H

#include <fcntl.h>
#include <netdb.h>
#include <stddef.h>
#include <sys/cdefs.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sentinel return value used by socket helpers to signal a non-blocking
 *        operation would block (EAGAIN / EWOULDBLOCK) and should be retried
 *        after readiness notification.
 *
 * We deliberately use a dedicated negative value (-2) distinct from the generic
 * error return (-1) to remove ambiguity for callers: -1 now always means a
 * hard error (check errno), while PO_SOCKET_WOULDBLOCK means transient.
 */
#define PO_SOCKET_WOULDBLOCK (-2)

/**
 * @brief Create, bind, and listen on a TCP socket.
 *
 * The function resolves the provided address (NULL or empty string means
 * INADDR_ANY) and binds to the given port. The returned socket is non-blocking
 * and has CLOEXEC set.
 *
 * @param[in] address IP address or hostname to bind (NULL or "" for any).
 * @param[in] port Port string (e.g. "8080").
 * @param[in] backlog Listen backlog.
 * @return Listening socket fd on success, -1 on error (errno set).
 * @note Thread-safe: Yes.
 */
int po_socket_listen(const char *address, const char *port, int backlog) __nonnull((2));

/**
 * @brief Connect to a remote TCP server.
 *
 * Creates a non-blocking socket and starts the connection process. A
 * non-blocking connect may return -1 with errno==EINPROGRESS to indicate
 * the connect is in progress; callers should monitor the socket for write
 * readiness to detect completion.
 *
 * @param[in] address Remote host (IP or hostname).
 * @param[in] port Remote port string.
 * @return Connected socket fd (non-blocking) on success, -1 on immediate error.
 * @note Thread-safe: Yes.
 */
int po_socket_connect(const char *address, const char *port) __nonnull((1, 2));

/**
 * @brief Accept a new connection on a listening socket.
 *
 * Uses accept4 where available to atomically set CLOEXEC & non-blocking.
 *
 * The returned client socket is non-blocking and has CLOEXEC set. If
 * out_addr_buf is non-NULL it will be populated with the textual peer
 * address (IPv4/IPv6). The function handles EINTR and EAGAIN correctly.
 *
 * @param[in] listen_fd Listening socket fd.
 * @param[out] out_addr_buf Buffer for textual peer address or NULL.
 * @param[in] addr_buf_len Length of out_addr_buf if provided.
 * @return New client fd on success, -1 on error, -2 if no pending connections
 *         (EAGAIN/EWOULDBLOCK).
 * @note Thread-safe: Yes (Concurrent accepts on same listen_fd are safe).
 */
int po_socket_accept(int listen_fd, char *out_addr_buf, size_t addr_buf_len);

/**
 * @brief Create, bind, and listen on a Unix domain socket path.
 *
 * The path will be unlinked before bind where appropriate. Permissions and
 * SELinux labels are the caller's responsibility. The socket is non-blocking.
 *
 * @param[in] path Filesystem path or abstract namespace (if starts with '\0').
 * @param[in] backlog Listen backlog.
 * @return Listening socket fd on success, -1 on error.
 * @note Thread-safe: Yes.
 */
int po_socket_listen_unix(const char *path, int backlog) __nonnull((1));

/**
 * @brief Connect to a Unix domain socket path.
 *
 * Returns a non-blocking connected socket (or EINPROGRESS semantics for
 * non-blocking connect).
 *
 * @param[in] path Filesystem path or abstract namespace (if starts with '\0').
 * @return Non-blocking socket fd on success, -1 on error.
 * @note Thread-safe: Yes.
 */
int po_socket_connect_unix(const char *path) __nonnull((1));

/**
 * @brief Close a socket and ignore EINTR.
 *
 * This helper centralizes FD closing semantics used across the net stack.
 *
 * @param[in] fd File descriptor to close.
 * @note Thread-safe: Yes (OS handles fd table).
 */
void po_socket_close(int fd);

/**
 * @brief Set a socket to non-blocking mode.
 *
 * @param[in] fd Socket fd.
 * @return 0 on success, -1 on error (errno set).
 * @note Thread-safe: Yes.
 */
int po_socket_set_nonblocking(int fd);

/**
 * @brief Set common socket options (TCP_NODELAY, REUSEADDR, KEEPALIVE).
 *
 * This helper is a convenience wrapper that sets several recommended options
 * for high-performance servers. It does not enable SO_LINGER unless
 * explicitly requested by the caller.
 *
 * @param[in] fd Socket fd.
 * @param[in] enable_nodelay Set TCP_NODELAY if non-zero.
 * @param[in] reuseaddr Set SO_REUSEADDR if non-zero.
 * @param[in] keepalive Set SO_KEEPALIVE if non-zero.
 * @return 0 on success, -1 on error (errno set).
 * @note Thread-safe: Yes.
 */
int po_socket_set_common_options(int fd, int enable_nodelay, int reuseaddr, int keepalive);

/**
 * @brief Send bytes on a non-blocking socket with metrics instrumentation.
 *
 * Wrapper around send(2) that records success/failure counters and bytes
 * transferred. EAGAIN/EWOULDBLOCK are treated as a transient condition and
 * reported to the caller via a -1 return (errno is preserved) while a hard
 * failure also returns -1 after incrementing a failure counter. Partial
 * writes are returned directly (short writes are possible with non-blocking
 * sockets and should be handled by the caller).
 *
 * @param[in] fd   Socket file descriptor.
 * @param[in] buf  Data buffer to write (must not be NULL if len > 0).
 * @param[in] len  Number of bytes to attempt to send.
 * @param[in] flags send(2) flags (e.g. MSG_NOSIGNAL, 0).
 * @return Number of bytes sent (>0) on success; 0 is never returned; -1 on
 *         hard error (errno set); PO_SOCKET_WOULDBLOCK (-2) if the operation
 *         would block and should be retried later.
 * @note Thread-safe: Yes (But interleaved bytes possible on shared FD).
 */
ssize_t po_socket_send(int fd, const void *buf, size_t len, int flags) __nonnull((2));

/**
 * @brief Receive bytes from a non-blocking socket with metrics instrumentation.
 *
 * Wrapper around recv(2) that records success/failure/EOF counters and bytes
 * transferred. On EAGAIN/EWOULDBLOCK it returns -1 (errno preserved) allowing
 * caller to retry after readiness notification. On EOF (peer orderly close)
 * it returns 0 and increments an EOF counter. On error it returns -1 after
 * incrementing a failure counter. Successful positive byte counts increment
 * both a recv counter and byte accumulator metric.
 *
 * @param[in] fd   Socket file descriptor.
 * @param[out] buf  Destination buffer (must not be NULL if len > 0).
 * @param[in] len  Maximum number of bytes to read.
 * @param[in] flags recv(2) flags.
 * @return >0 number of bytes read, 0 on EOF, -1 on hard error (errno set),
 *         PO_SOCKET_WOULDBLOCK (-2) if the operation would block.
 * @note Thread-safe: Yes.
 */
ssize_t po_socket_recv(int fd, void *buf, size_t len, int flags) __nonnull((2));

#ifdef __cplusplus
}
#endif

#endif /* _SOCKET_H */