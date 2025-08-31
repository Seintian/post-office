/**
 * @file socket.h
 * @brief TCP and UNIX-domain socket utility functions.
 *
 * This header provides minimal socket helpers used by the PostOffice net
 * implementation. All sockets returned by these helpers are configured with
 * CLOEXEC and set to non-blocking mode by default.
 */

#ifndef _SOCKET_H
#define _SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/cdefs.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create, bind, and listen on a TCP socket.
 *
 * The function resolves the provided address (NULL or empty string means
 * INADDR_ANY) and binds to the given port. The returned socket is non-blocking
 * and has CLOEXEC set.
 *
 * @param address IP address or hostname to bind (NULL or "" for any).
 * @param port Port string (e.g. "8080").
 * @param backlog Listen backlog.
 * @return Listening socket fd on success, -1 on error (errno set).
 */
int socket_listen(const char *address, const char *port, int backlog) __nonnull((2));

/**
 * @brief Connect to a remote TCP server.
 *
 * Creates a non-blocking socket and starts the connection process. A
 * non-blocking connect may return -1 with errno==EINPROGRESS to indicate
 * the connect is in progress; callers should monitor the socket for write
 * readiness to detect completion.
 *
 * @param address Remote host (IP or hostname).
 * @param port Remote port string.
 * @return Connected socket fd (non-blocking) on success, -1 on immediate error.
 */
int socket_connect(const char *address, const char *port) __nonnull((1,2));

/**
 * @brief Accept a new connection on a listening socket.
 *
 * The returned client socket is non-blocking and has CLOEXEC set. If
 * out_addr_buf is non-NULL it will be populated with the textual peer
 * address (IPv4/IPv6). The function handles EINTR and EAGAIN correctly.
 *
 * @param listen_fd Listening socket fd.
 * @param out_addr_buf Buffer for textual peer address or NULL.
 * @param addr_buf_len Length of out_addr_buf if provided.
 * @return New client fd on success, -1 on error, -2 if no pending connections
 *         (EAGAIN/EWOULDBLOCK).
 */
int socket_accept(int listen_fd, char *out_addr_buf, size_t addr_buf_len);

/**
 * @brief Create, bind, and listen on a Unix domain socket path.
 *
 * The path will be unlinked before bind where appropriate. Permissions and
 * SELinux labels are the caller's responsibility. The socket is non-blocking.
 *
 * @param path Filesystem path or abstract namespace (if starts with '\0').
 * @param backlog Listen backlog.
 * @return Listening socket fd on success, -1 on error.
 */
int socket_listen_unix(const char *path, int backlog) __nonnull((1));

/**
 * @brief Connect to a Unix domain socket path.
 *
 * Returns a non-blocking connected socket (or EINPROGRESS semantics for
 * non-blocking connect).
 *
 * @param path Filesystem path or abstract namespace (if starts with '\0').
 * @return Non-blocking socket fd on success, -1 on error.
 */
int socket_connect_unix(const char *path) __nonnull((1));

/**
 * @brief Close a socket and ignore EINTR.
 *
 * This helper centralizes FD closing semantics used across the net stack.
 *
 * @param fd File descriptor to close.
 */
void socket_close(int fd);

/**
 * @brief Set a socket to non-blocking mode.
 *
 * @param fd Socket fd.
 * @return 0 on success, -1 on error (errno set).
 */
int socket_set_nonblocking(int fd);

/**
 * @brief Set common socket options (TCP_NODELAY, REUSEADDR, KEEPALIVE).
 *
 * This helper is a convenience wrapper that sets several recommended options
 * for high-performance servers. It does not enable SO_LINGER unless
 * explicitly requested by the caller.
 *
 * @param fd Socket fd.
 * @param enable_nodelay Set TCP_NODELAY if non-zero.
 * @param reuseaddr Set SO_REUSEADDR if non-zero.
 * @param keepalive Set SO_KEEPALIVE if non-zero.
 * @return 0 on success, -1 on error (errno set).
 */
int socket_set_common_options(int fd, int enable_nodelay, int reuseaddr, int keepalive);

#ifdef __cplusplus
}
#endif

#endif /* _SOCKET_H */