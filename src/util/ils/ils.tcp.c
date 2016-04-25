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

#include <ils/util/everything.h>

#include <tr50/error.h>

#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/tcp.h>

#define TR50_TCP_CONNECT_TIMEOUT_IN_MS		15000

int g_tr50_tcp_ssl_init = 0;
int g_tr50_tcp_ssl_verify_peer = 0;
char g_tr50_tcp_ssl_password[128];
char g_tr50_tcp_ssl_file[256];

int _tcp_connect_ssl(void *handle);

int _tcp_connect(void **sock, const char *addr, long port, int options) {
	int ret;
	void *handle;

	socket_create(&handle);

	if ((ret = socket_connect_ex(handle, format("%s:%d", addr, port), TR50_TCP_CONNECT_TIMEOUT_IN_MS)) != 0) {
		socket_delete(handle);
	} else {
		if (options & TCP_OPTION_SECURE) {
			if ((ret = _tcp_connect_ssl(handle)) != 0) {
				socket_delete(handle);
			} else {
				*sock = handle;
			}
		} else {
			*sock = handle;
		}
	}

	return ret;
}

int _tcp_connect_proxy(void **sock, const char *addr, long port, int options, int proxy_type, const char *proxy_addr, const char *proxy_username, const char *proxy_password) {
	int ret;
	void *handle;
	SOCKET_PROXY_INFO proxyinfo;

	ilsMemset(&proxyinfo, 0, sizeof(SOCKET_PROXY_INFO));

	switch (proxy_type) {
	case TCP_PROXY_TYPE_HTTP:
		proxyinfo.type = SOCKET_PROXY_HTTP;
		break;
	case TCP_PROXY_TYPE_SOCK4:
		proxyinfo.type = SOCKET_PROXY_SOCKS4;
		break;
	case TCP_PROXY_TYPE_SOCK4A:
		proxyinfo.type = SOCKET_PROXY_SOCKS4A;
		break;
	case TCP_PROXY_TYPE_SOCK5:
		proxyinfo.type = SOCKET_PROXY_SOCKS5;
		break;
	default:
		return ERR_TCP_PROXY_TYPE_UNKNOWN;
	}

	proxyinfo.addr = (char *)proxy_addr;
	proxyinfo.user = (char *)proxy_username;
	proxyinfo.pass = (char *)proxy_password;

	socket_create(&handle);

	if ((ret = socket_connect_thru_proxy(handle, &proxyinfo, format("%s:%d", addr, port), 30000)) != 0) {
		socket_delete(handle);
	} else {
		if (options & TCP_OPTION_SECURE) {
			if ((ret = _tcp_connect_ssl(handle)) != 0) {
				socket_delete(handle);
			} else {
				*sock = handle;
			}
		} else {
			*sock = handle;
		}
	}

	return ret;
}

int _tcp_ssl_config(const char *password, const char *file, int verify_peer) {
	if (g_tr50_tcp_ssl_init == 0) {
		ssl_init();
	}
	_memory_memset(g_tr50_tcp_ssl_password, 0, 128);
	_memory_memset(g_tr50_tcp_ssl_file, 0, 256);

	if (password) {
		strncpy(g_tr50_tcp_ssl_password, password, 127);
	}

	if (file) {
		strncpy(g_tr50_tcp_ssl_file, file, 255);
	}

	g_tr50_tcp_ssl_verify_peer = verify_peer;
	g_tr50_tcp_ssl_init = 1;

	return 0;
}

int _tcp_connect_ssl(void *handle) {
	int ret;
	void *ctx;

	if (g_tr50_tcp_ssl_init == 0) {
		_memory_memset(g_tr50_tcp_ssl_password, 0, 128);
		_memory_memset(g_tr50_tcp_ssl_file, 0, 256);
	}

	if ((ret = ssl_ctx_create(&ctx)) != 0) {
		return ret;
	}

	if (strlen(g_tr50_tcp_ssl_password) > 0) {
		if ((ret = ssl_ctx_set_passwd(ctx, g_tr50_tcp_ssl_password)) != 0) {
			log_debug("_tcp_connect_ssl(): ssl_ctx_set_passwd(): %d.", ret);
			log_debug("_tcp_connect_ssl(): ssl_ctx_set_passwd(): %s.", ssl_ctx_error(ctx));
		} else {
			log_debug("_tcp_connect_ssl(): ssl_ctx_set_passwd(): OK.");
		}
	}

	if (strlen(g_tr50_tcp_ssl_file) > 0) {
		if ((ret = ssl_ctx_set_packed_file(ctx, g_tr50_tcp_ssl_file)) != 0) {
			log_debug("_tcp_connect_ssl(): ssl_ctx_set_packed_file(): %d.", ret);
			log_debug("_tcp_connect_ssl(): ssl_ctx_set_packed_file(): %s.", ssl_ctx_error(ctx));
		} else {
			log_debug("_tcp_connect_ssl(): ssl_ctx_set_packed_file(): OK.");
		}
	}

	if (g_tr50_tcp_ssl_verify_peer) {
		if ((ret = ssl_ctx_set_verify(ctx, 1, 1)) != 0) {
			log_debug("_tcp_connect_ssl(): ssl_ctx_set_verify(): %d.", ret);
		} else {
			log_debug("_tcp_connect_ssl(): ssl_ctx_set_verify(): OK.");
		}
	}

	if ((ret = ssl_connect(handle, ctx)) != 0) {
		log_debug("_tcp_connect_ssl(): ssl_connect(): %d (%s)(%s).", ret, ssl_error(handle) ? ssl_error(handle) : "NULL", ssl_ctx_error(ctx) ? ssl_ctx_error(ctx) : "NULL");
		ssl_ctx_delete(ctx);
		return ret;
	}

	ssl_ctx_delete(ctx);

	log_debug("_tcp_connect_ssl(): SSL Connection Established.");

	return 0;
}

int _tcp_disconnect(void *sock) {
	socket_shutdown(sock);
	socket_delete(sock);

	return 0;
}

int _tcp_send(void *sock, const char *buf, int len, int timeout) {
	int ret = socket_send2(sock, buf, len, timeout);
	if (ret == 0) {
		log_hexdump(LOG_TYPE_LOW_LEVEL, "_tcp_send(): sent", buf, len);
	}
	if (ret == ERR_SOCK_TIMEOUT) {
		return ERR_TR50_TCP_TIMEOUT;
	}

	return ret;
}

int _tcp_recv(void *sock, char *buf, int *len, int timeout) {
	int ret = socket_recv2(sock, buf, len, timeout);

	if (ret != 0 && ret != ERR_SOCK_TIMEOUT) {
		log_important_info("socket_recv2(): failed [%d] ex[%d]", ret, socket_error(sock));
	}

	if (ret == 0) {
		log_hexdump(LOG_TYPE_LOW_LEVEL, "_tcp_recv(): recv'ed", buf, *len);
	}
	if (ret == ERR_SOCK_TIMEOUT) {
		return ERR_TR50_TCP_TIMEOUT;
	}

	return ret;
}
