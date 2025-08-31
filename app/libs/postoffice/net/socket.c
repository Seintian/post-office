/**
 * @file socket.c
 * @brief TCP and UNIX-domain socket helpers (non-blocking, CLOEXEC).
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "net/socket.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/tcp.h>
#include <sys/un.h>

static int set_cloexec_nonblock(int fd) {
	if (fd < 0) return -1;
#ifdef O_NONBLOCK
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) return -1;
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
#else
	(void)fd;
#endif
#ifdef FD_CLOEXEC
	int fdflags = fcntl(fd, F_GETFD, 0);
	if (fdflags < 0) return -1;
	if (fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0) return -1;
#endif
	return 0;
}

int socket_set_nonblocking(int fd) {
	if (fd < 0) {
		errno = EINVAL;
		return -1;
	}
	return set_cloexec_nonblock(fd);
}

int socket_set_common_options(int fd, int enable_nodelay, int reuseaddr, int keepalive) {
	if (fd < 0) {
		errno = EINVAL;
		return -1;
	}

	int rc = 0;
	if (reuseaddr) {
		int v = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v)) < 0)
			rc = -1;
	}

	if (keepalive) {
		int v = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &v, sizeof(v)) < 0)
			rc = -1;
	}

	if (enable_nodelay) {
		int v = 1;
		if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v)) < 0)
			rc = -1;
	}

	return rc;
}

int socket_listen(const char *address, const char *port, int backlog) {

	struct addrinfo hints; memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_UNSPEC;     // allow IPv4/IPv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_PASSIVE;    // for bind

	struct addrinfo *res = NULL;
	int gai = getaddrinfo((address && address[0]) ? address : NULL, port, &hints, &res);
	if (gai != 0) {
		errno = EINVAL;
		return -1;
	}

	int listen_fd = -1;
	for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
		int fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, ai->ai_protocol);
		if (fd < 0) continue;

		int one = 1;
		(void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

		if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 && listen(fd, backlog) == 0) {
			listen_fd = fd;
			// do not break; exit loop by setting ai = NULL below to avoid nested breaks
			ai = NULL; // ensure we exit
			// close remaining? We'll exit after loop
		} else {
			close(fd);
		}

		if (listen_fd != -1) break;
	}

	freeaddrinfo(res);
	return listen_fd; // -1 if none worked
}

int socket_connect(const char *address, const char *port) {

	struct addrinfo hints; memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *res = NULL;
	int gai = getaddrinfo(address, port, &hints, &res);
	if (gai != 0) { errno = EINVAL; return -1; }

	int fd_out = -1;
	for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
		int fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, ai->ai_protocol);
		if (fd < 0) continue;

		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0 || errno == EINPROGRESS) {
			fd_out = fd;
			break;
		}

		close(fd);
	}
	freeaddrinfo(res);
	return fd_out;
}

int socket_accept(int listen_fd, char *out_addr_buf, size_t addr_buf_len) {
	if (listen_fd < 0) { errno = EINVAL; return -1; }

	struct sockaddr_storage ss; socklen_t slen = sizeof(ss);
#ifdef SOCK_NONBLOCK
	int fd = accept4(listen_fd, (struct sockaddr*)&ss, &slen, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (fd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) return -2;
		return -1;
	}
#else
	int fd = accept(listen_fd, (struct sockaddr*)&ss, &slen);
	if (fd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) return -2;
		return -1;
	}
	if (set_cloexec_nonblock(fd) < 0) { close(fd); return -1; }
#endif

	if (out_addr_buf && addr_buf_len) {
		char host[NI_MAXHOST];
		char serv[NI_MAXSERV];
		if (getnameinfo((struct sockaddr*)&ss, slen,
						host, sizeof(host),
						serv, sizeof(serv),
						NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
			snprintf(out_addr_buf, addr_buf_len, "%s:%s", host, serv);
		} else if (addr_buf_len) {
			out_addr_buf[0] = '\0';
		}
	}

	return fd;
}

int socket_listen_unix(const char *path, int backlog) {

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) return -1;

	struct sockaddr_un sun; memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	size_t len = strnlen(path, sizeof(sun.sun_path));

	if (path[0] == '\0') {
		// abstract namespace (Linux): first byte remains 0
		memcpy(sun.sun_path, path, len);
	} else {
		// filesystem path: unlink if exists
		if (len >= sizeof(sun.sun_path)) {
			close(fd); errno = ENAMETOOLONG; return -1;
		}
		strncpy(sun.sun_path, path, sizeof(sun.sun_path) - 1);
		unlink(path);
	}

	socklen_t slen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + len);
	if (bind(fd, (struct sockaddr*)&sun, slen) < 0) { close(fd); return -1; }
	if (listen(fd, backlog) < 0) { close(fd); return -1; }
	return fd;
}

int socket_connect_unix(const char *path) {
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) return -1;

	struct sockaddr_un sun; memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	size_t len = strnlen(path, sizeof(sun.sun_path));
	if (path[0] == '\0') {
		memcpy(sun.sun_path, path, len);
	} else {
		if (len >= sizeof(sun.sun_path)) { close(fd); errno = ENAMETOOLONG; return -1; }
		strncpy(sun.sun_path, path, sizeof(sun.sun_path) - 1);
	}

	socklen_t slen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + len);
	if (connect(fd, (struct sockaddr*)&sun, slen) == 0) return fd;
	if (errno == EINPROGRESS) return fd;
	close(fd);
	return -1;
}

void socket_close(int fd) {
	if (fd >= 0) {
		while (close(fd) == -1 && errno == EINTR) {
			// retry
		}
	}
}

