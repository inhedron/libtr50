/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 ILS Technology, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <tr50/error.h>
#include <tr50/util/log.h>
#include <tr50/util/tcp.h>
#include <tr50/util/time.h>
#include <tr50/util/memory.h>

extern void _ssl_init();
extern int __ssl_send2(void *handle, const char *buffer, int length, int timeout);
extern int __ssl_recv2(void *handle, char *buffer, int *length, int timeout);
extern int _ssl_ctx_create(void **ctx);
extern int _ssl_ctx_delete(void *ctx);
extern int _ssl_ctx_set_verify(void *ctx, int require_peer_certificate, int verify_peer);
extern int _ssl_ctx_set_packed_file(void *ctx, const char *cfile);
extern int _ssl_ctx_set_passwd(void *ctx, const char *passwd);
extern const char *_ssl_error(void *handle);
extern const char *_ssl_ctx_error(void *ctx);
extern int _ssl_connect(void *handle, void *ctx);

extern int make_addr(const char *addr, struct sockaddr_in *sin);
extern int _socket_proxy_uses_hostname(const char *addr);
extern int _socket_proxy_setup_socks4(void *handle, const char *addr, const char *user);
extern int _socket_proxy_setup_socks4a(void *handle, const char *addr, const char *user);
extern int _socket_proxy_setup_socks5(void *handle, const char *addr, const char *user, const char *pass);
extern int _socket_proxy_setup_http(void *handle, const char *addr, const char *host, const char *user, const char *pass);

int _tcp_connect_ssl(void *handle);

#define SOCKET_TYPE_SOCK				1
#define TCP_DEFAULT_TIMEOUT				10000

#define SOCKET_PROXY_SOCKS4				1
#define SOCKET_PROXY_SOCKS4A			2
#define SOCKET_PROXY_SOCKS5				3
#define SOCKET_PROXY_HTTP				4

#define SOCKET_USE_NODELAY				1
#define SOCKET_DONT_SET_QUEUE_SIZES		2

#define DTCPBUF							65535

#define TCPCONNTIMEOUT					5000

typedef struct {
	int s;
	int type;
	int err;
	int is_ssl;
	void *sslo;
} ABSTRACT_SOCKET;

int g_tr50_tcp_ssl_init = 0;
void *g_tr50_tcp_ssl_password[128];
void *g_tr50_tcp_ssl_file[256];
int g_tr50_tcp_ssl_verify_peer = 0;

int _tcp_ssl_config(const char *password, const char *file, int verify_peer) {
	_memory_memset((void *)g_tr50_tcp_ssl_password, 0, 128);
	_memory_memset((void *)g_tr50_tcp_ssl_file, 0, 256);

	if (password) {
		strncpy((char *)g_tr50_tcp_ssl_password, password, 127);
	}

	if (file) {
		strncpy((char *)g_tr50_tcp_ssl_file, file, 255);
	}

	g_tr50_tcp_ssl_verify_peer = verify_peer;

	return 0;
}

int _tcp_connect_proxy(void **sock, const char *addr, long port, int options, int proxy_type, const char *proxy_addr, const char *proxy_username, const char *proxy_password) {
	int ret;
	int timeout = TCP_DEFAULT_TIMEOUT;
	char buffer[64];

	if (strchr(proxy_addr, ':') == NULL) {
		if (proxy_type == SOCKET_PROXY_HTTP) {
			sprintf(buffer, "%s:80", proxy_addr);
			if ((ret = _tcp_connect(sock, buffer, timeout, SOCKET_USE_NODELAY)) != 0) {
				return ERR_TR50_PROXY_CONN_FAILED;
			}
		} else {
			sprintf(buffer, "%s:1080", proxy_addr);
			if ((ret = _tcp_connect(sock, buffer, timeout, SOCKET_USE_NODELAY)) != 0) {
				return ERR_TR50_PROXY_CONN_FAILED;
			}
		}
	} else {
		if ((ret = _tcp_connect(sock, proxy_addr, timeout, SOCKET_USE_NODELAY)) != 0) {
			return ERR_TR50_PROXY_CONN_FAILED;
		}
	}

	if (proxy_type == SOCKET_PROXY_SOCKS4 || proxy_type == SOCKET_PROXY_SOCKS4A) {
		if (_socket_proxy_uses_hostname(addr)) {
			ret = _socket_proxy_setup_socks4a(sock, addr, proxy_username);
		} else {
			ret = _socket_proxy_setup_socks4(sock, addr, proxy_username);
		}
	} else if (proxy_type == SOCKET_PROXY_SOCKS5) {
		ret = _socket_proxy_setup_socks5(sock, addr, proxy_username, proxy_password);
	} else if (proxy_type == SOCKET_PROXY_HTTP) {
		ret = _socket_proxy_setup_http(sock, addr, proxy_addr, proxy_username, proxy_password);
	} else {
		ret = ERR_TR50_PROXY_PROTO_BAD;
	}

	if (ret != 0) {
		_tcp_disconnect(sock);
	}

	return ret;
}

int _tcp_connect(void **handle, const char *addr, long port, int options) {
	ABSTRACT_SOCKET *sock;
	struct linger llinger;
	int count = 0;
	int tint;
	struct sockaddr_in sa;
	int ret;
	int fmask = 0;
	char buffer[512];
	struct timeval t;
	int timeout = TCPCONNTIMEOUT;
	fd_set wr, xc;
	int many;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if ((sock = _memory_malloc(sizeof(ABSTRACT_SOCKET))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(sock, 0, sizeof(ABSTRACT_SOCKET));

	llinger.l_onoff = 1;
	llinger.l_linger = 0;

	sock->type = SOCKET_TYPE_SOCK;

	while ((sock->s = (int)socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0 && count++ < 10)
		; /* Nothing. */

	if (count >= 10) {
		sock->err = errno;
		_tcp_disconnect(sock);
		return ERR_TR50_SOCK_SOCKET_FAILED;
	}

	tint = 1;

	if ((options & SOCKET_USE_NODELAY) == SOCKET_USE_NODELAY) {
		if (setsockopt(sock->s, IPPROTO_TCP, TCP_NODELAY, (char *)&tint, sizeof(tint)) != 0) {
			sock->err = errno;
			_tcp_disconnect(sock);
			return ERR_TR50_SOCK_SETSOCKOPT_FAILED;
		}
	}

	if (setsockopt(sock->s, SOL_SOCKET, SO_KEEPALIVE, (char *)&tint, sizeof(tint)) != 0) {
		sock->err = errno;
		_tcp_disconnect(sock);
		return ERR_TR50_SOCK_SETSOCKOPT_FAILED;
	}

	if (setsockopt(sock->s, SOL_SOCKET, SO_OOBINLINE, (char *)&tint, sizeof(tint)) != 0) {
		sock->err = errno;
	}

	if (setsockopt(sock->s, SOL_SOCKET, SO_LINGER, (char *)&llinger, sizeof(llinger)) != 0) {
		sock->err = errno;
		_tcp_disconnect(sock);
		return ERR_TR50_SOCK_SETSOCKOPT_FAILED;
	}

	if ((options & SOCKET_DONT_SET_QUEUE_SIZES) == SOCKET_DONT_SET_QUEUE_SIZES) {
		tint = DTCPBUF;
		if (setsockopt(sock->s, SOL_SOCKET, SO_SNDBUF, (char *)&tint, sizeof(tint)) != 0) {
			sock->err = errno;
			_tcp_disconnect(sock);
			return ERR_TR50_SOCK_SETSOCKOPT_FAILED;
		}

		if (setsockopt(sock->s, SOL_SOCKET, SO_RCVBUF, (char *)&tint, sizeof(tint)) != 0) {
			sock->err = errno;
			_tcp_disconnect(sock);
			return ERR_TR50_SOCK_SETSOCKOPT_FAILED;
		}
	}

	if (fcntl(sock->s, F_SETFD, fcntl(sock->s, F_GETFD) | FD_CLOEXEC) != 0) {
		sock->err = errno;
	}

	if (fcntl(sock->s, F_SETFL, ((fmask = fcntl(sock->s, F_GETFL, 0)) | O_NONBLOCK)) < 0) {
		sock->err = errno;
		_tcp_disconnect(sock);
		return ERR_TR50_SOCK_FNCTL_FAILED;
	}

	sprintf(buffer, "%s:%d", addr, (int)port);

	if ((ret = make_addr(buffer, &sa)) != 0) {
		sock->err = ret;
		_tcp_disconnect(sock);
		return ret;
	}

	if ((ret = connect(sock->s, (struct sockaddr *)&sa, sizeof(sa))) < 0) {
		if (errno != EINPROGRESS) {
			sock->err = errno;
			_tcp_disconnect(sock);
			return ERR_TR50_SOCK_CONNECT_FAILED;
		}
	}

	if (fcntl(sock->s, F_SETFL, (fmask &= ~O_NONBLOCK)) < 0) {
		sock->err = errno;
		_tcp_disconnect(sock);
		return ERR_TR50_SOCK_FNCTL_FAILED;
	}

	FD_ZERO(&wr);
	FD_ZERO(&xc);
	FD_SET(sock->s, &wr);
	FD_SET(sock->s, &xc);
	t.tv_sec = timeout / 1000;
	t.tv_usec = (timeout % 1000) * 1000;

	if ((many = select(sock->s + 1, NULL, &wr, &xc, &t)) <= 0) {
		if (many == 0) {
			sock->err = ERR_TR50_TIMEOUT;
			_tcp_disconnect(sock);
			return ERR_TR50_TIMEOUT;
		} else {
			sock->err = errno;
			_tcp_disconnect(sock);
			return ERR_TR50_SOCK_SELECT_FAILED;
		}
	}

	if (FD_ISSET(sock->s, &xc)) {
		sock->err = errno;
		_tcp_disconnect(sock);
		return ERR_TR50_SOCK_CONNECT_FAILED;
	}

	if (FD_ISSET(sock->s, &wr)) {
		int err = 0;
		int len = sizeof(int);

		if (getsockopt(sock->s, SOL_SOCKET, SO_ERROR, (char *)&err, (socklen_t *)&len) < 0) {
			sock->err = errno;
			_tcp_disconnect(sock);
			return ERR_TR50_SOCK_GETSOCKOPT_FAILED;
		}

		if (err != 0) {
			sock->err = err;
			_tcp_disconnect(sock);
			return ERR_TR50_SOCK_OTHER;
		}

		if (options & TCP_OPTION_SECURE) {
			if ((ret = _tcp_connect_ssl(sock)) != 0) {
				_tcp_disconnect(sock);
				return ret;
			}
		}

		*handle = sock;
		return 0;
	}

	sock->err = errno;
	_tcp_disconnect(handle);

	return ERR_TR50_SOCK_OTHER;
}

int _tcp_disconnect(void *handle) {
	ABSTRACT_SOCKET *sock = handle;
	int ret;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (sock->type != SOCKET_TYPE_SOCK) {
		ret = 0;
	}

	if (sock->s == 0) {
		log_important_info("_tcp_disconnect(): failed[socket is zero]\n");
		ret = ERR_TR50_SOCK_IS_ZERO;
	} else {
		if (close(sock->s) < 0) {
			sock->err = errno;
			log_important_info("_tcp_disconnect(): failed [%d]\n", sock->err);
			sock->s = 0;
			return ERR_TR50_SOCK_OTHER;
		}
		sock->s = 0;
	}

	_memory_free(sock);

	return ret;
}

int _tcp_send(void *handle, const char *buffer, int length, int timeout) {
	ABSTRACT_SOCKET *sock = handle;
	fd_set	sockSet;
	int current_sent = 0;
	struct timeval to;
	int ret;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	log_hexdump(LOG_TYPE_LOW_LEVEL, "_tcp_send()", buffer, length);

	if (sock->is_ssl) {
		return __ssl_send2(handle, buffer, length, timeout);
	}

	if (length == -1) {
		length = strlen(buffer) + 1;
	}

	if (timeout < -1) {
		sock->err = errno;
		return ERR_TR50_SOCK_OTHER;
	} else {
		to.tv_usec = (timeout % 1000) * 1000;
		to.tv_sec = timeout / 1000;
	}

	while (1) {
		if (timeout > 0) {
			FD_ZERO(&sockSet);
			FD_SET(sock->s, &sockSet);

			if ((ret = select(sock->s + 1, NULL, &sockSet, NULL, &to)) == 0) {
				sock->err = ERR_TR50_SOCK_TIMEOUT;
				return ERR_TR50_SOCK_TIMEOUT;
			} else if (ret == -1) {
				sock->err = errno;
				return ERR_TR50_SOCK_SELECT_FAILED;
			}
		}

		if (length - current_sent > DTCPBUF) {
			ret = send(sock->s, buffer + current_sent, DTCPBUF, MSG_NOSIGNAL);
		} else {
			ret = send(sock->s, buffer + current_sent, length - current_sent, MSG_NOSIGNAL);
		}

		if ((ret + current_sent) == length) {
			return 0;
		} else if (ret == -1) {
			sock->err = errno;
			return ERR_TR50_SOCK_SEND_FAILED;
		} else {
			current_sent += ret;
		}
	}
}

int _tcp_recv(void *handle, char *buffer, int *length, int timeout) {
	ABSTRACT_SOCKET *sock = handle;
	fd_set	sockSet;
	struct timeval to;
	int ret;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (sock->is_ssl) {
		return __ssl_recv2(handle, buffer, length, timeout);
	}

	if (timeout < -1) {
		sock->err = errno;
		return ERR_TR50_SOCK_BAD_TIMEOUT;
	}

	if (timeout > 0) {
		to.tv_usec = (timeout % 1000) * 1000;
		to.tv_sec = timeout / 1000;

		FD_ZERO(&sockSet);
		FD_SET(sock->s, &sockSet);
		if ((ret = select(sock->s + 1, &sockSet, NULL, NULL, &to)) == 0) {
			sock->err = ERR_TR50_SOCK_TIMEOUT;
			return ERR_TR50_SOCK_TIMEOUT;
		} else if (ret == -1) {
			sock->err = errno;
			return ERR_TR50_SOCK_SELECT_FAILED;
		}
	}

	ret = recv(sock->s, buffer, *length, MSG_NOSIGNAL);

	if (ret == -1) {
		sock->err = errno;
		*length = 0;
		return ERR_TR50_SOCK_RECV_FAILED;
	} else if (ret == 0) {
		sock->err = ERR_TR50_SOCK_SHUTDOWN;
		*length = 0;
		return ERR_TR50_SOCK_SHUTDOWN;
	} else {
		*length = ret;
		log_hexdump(LOG_TYPE_LOW_LEVEL, "_tcp_recv()", buffer, ret);
		return 0;
	}
}

int _tcp_connect_ssl(void *handle) {
	int ret;
	void *ctx;

	if (g_tr50_tcp_ssl_init != 1) {
		_ssl_init();
		g_tr50_tcp_ssl_init = 1;
	}

	if ((ret = _ssl_ctx_create(&ctx)) != 0) {
		return ret;
	}

	if (strlen((const char *)g_tr50_tcp_ssl_password) > 0) {
		if ((ret = _ssl_ctx_set_passwd(ctx, (const char *)g_tr50_tcp_ssl_password)) != 0) {
			log_important_info("_tcp_connect_ssl(): ssl_ctx_set_passwd(): %d.", ret);
			log_important_info("_tcp_connect_ssl(): ssl_ctx_set_passwd(): %s.", _ssl_ctx_error(ctx));
		} else {
			log_important_info("_tcp_connect_ssl(): ssl_ctx_set_passwd(): OK.");
		}
	}

	if (strlen((const char *)g_tr50_tcp_ssl_file) > 0) {
		if ((ret = _ssl_ctx_set_packed_file(ctx, (const char *)g_tr50_tcp_ssl_file)) != 0) {
			log_important_info("_tcp_connect_ssl(): _ssl_ctx_set_packed_file(): %d.", ret);
			log_important_info("_tcp_connect_ssl(): _ssl_ctx_set_packed_file(): %s.", _ssl_ctx_error(ctx));
		} else {
			log_important_info("_tcp_connect_ssl(): _ssl_ctx_set_packed_file(): OK.");
		}
	}

	if (g_tr50_tcp_ssl_verify_peer) {
		if ((ret = _ssl_ctx_set_verify(ctx, 1, 1)) != 0) {
			log_important_info("_tcp_connect_ssl(): _ssl_ctx_set_verify(): %d.", ret);
		} else {
			log_important_info("_tcp_connect_ssl(): _ssl_ctx_set_verify(): OK.");
		}
	}

	if ((ret = _ssl_connect(handle, ctx)) != 0) {
		log_important_info("_tcp_connect_ssl(): _ssl_connect(): %d (%s)(%s).", ret, _ssl_error(handle) ? _ssl_error(handle) : "NULL", _ssl_ctx_error(ctx) ? _ssl_ctx_error(ctx) : "NULL");
		_ssl_ctx_delete(ctx);
		return ret;
	}

	_ssl_ctx_delete(ctx);

	log_important_info("_tcp_connect_ssl(): SSL Connection Established.");

	return 0;
}
