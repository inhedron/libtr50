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

#include <Windows.h>

#include <tr50/error.h>

#include <tr50/util/blob.h>
#include <tr50/util/memory.h>
#include <tr50/util/tcp.h>

#define TCP_DEFAULT_TIMEOUT 10000

typedef struct {
	int s;
	int type;
	int err;
	int is_ssl;
	void *sslo;
} ABSTRACT_SOCKET;

const char g_base64_charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char g_base64_default_pad_char = '=';

int make_addr(const char *addr, struct sockaddr_in *sin) {
	char *colon;
	struct hostent *hoste;

	if ((addr == NULL) || (sin == NULL)) {
		return ERR_TR50_PARMS;
	}
	_memory_memset(sin, 0, sizeof(struct sockaddr_in));
	sin->sin_family = AF_INET;

	colon = strchr(addr, ':');
	if (colon != NULL) {
		*colon = '\0';
	}

	if ((hoste = gethostbyname(addr)) == NULL) {
		return ERR_TR50_SOCK_HOSTNOTFOUND;
	}
	if (&(sin->sin_addr) == NULL) {
		return ERR_TR50_SOCK_HOSTNOTFOUND;
	}
	if (hoste->h_addr_list[0] == NULL) {
		return ERR_TR50_SOCK_HOSTNOTFOUND;
	}
	_memory_memcpy(&(sin->sin_addr), hoste->h_addr_list[0], hoste->h_length);

	if (colon != NULL) {
		sin->sin_port = htons(atoi(colon + 1));
		*colon = ':';
	} else {
		sin->sin_port = 0;
	}
	return 0;
}

int base64_encode(const unsigned char *in, int inlen, char **out) {
	int outlen;
	size_t datalength = 0;
	unsigned char input[3];
	unsigned char output[4];
	size_t i;
	char *tmp;

	//This was the base64_sizeof(int asciiSize) function
	inlen = inlen * 4 / 3;
	inlen += 4 - inlen % 4;
	outlen = inlen + 1;

	if ((*out = _memory_malloc(outlen)) == NULL) {
		return -1;
	}
	tmp = *out;

	while (2 < inlen) {
		input[0] = *in++;
		input[1] = *in++;
		input[2] = *in++;
		inlen -= 3;

		output[0] = input[0] >> 2;
		output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
		output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
		output[3] = input[2] & 0x3f;

		tmp[datalength++] = g_base64_charset[output[0]];
		tmp[datalength++] = g_base64_charset[output[1]];
		tmp[datalength++] = g_base64_charset[output[2]];
		tmp[datalength++] = g_base64_charset[output[3]];
	}

	/* Now we worry about padding. */
	if (0 != inlen) {
		/* Get what's left. */
		input[0] = input[1] = input[2] = '\0';
		for (i = 0; (int)i < inlen; ++i) {
			input[i] = *in++;
		}

		output[0] = input[0] >> 2;
		output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
		output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);

		tmp[datalength++] = g_base64_charset[output[0]];
		tmp[datalength++] = g_base64_charset[output[1]];
		if (inlen == 1) {
			tmp[datalength++] = g_base64_default_pad_char;
		} else {
			tmp[datalength++] = g_base64_charset[output[2]];
		}
		tmp[datalength++] = g_base64_default_pad_char;
	}

	tmp[datalength] = '\0';    /* Returned value doesn't count \0. */
	*out = tmp;
	return 0;
}

int socket_error(const void *socket) {
	const ABSTRACT_SOCKET *s = socket;
	if (socket == NULL) {
		return ERR_TR50_BADHANDLE;
	}
	return s->err;
}

int _socket_proxy_uses_hostname(const char *addr) {
	int idx = 0;

	while (addr[idx] != '\0') {
		if (isdigit(addr[idx]) || addr[idx] == '.' || addr[idx] == ':') {
			idx++;
		} else {
			return TRUE;
		}
	}

	return FALSE;
}

int _socket_proxy_setup_socks4(void *handle, const char *addr, const char *user) {
	char buf[9];
	struct sockaddr_in sin;
	int ret;
	unsigned short i_port;
	unsigned int   i_addr;

	if ((ret = make_addr(addr, &sin)) != 0) {
		return ret;
	}
	i_port = sin.sin_port;

	i_addr = ((unsigned int)sin.sin_addr.S_un.S_addr);

	buf[0] = 0x04;
	buf[1] = 0x01;
	_memory_memcpy(buf + 2, &i_port, sizeof(short));
	_memory_memcpy(buf + 4, &i_addr, sizeof(int));
	buf[8] = 0x00;

	if (user == NULL) {
		if ((ret = _tcp_send(handle, buf, 9, TCP_DEFAULT_TIMEOUT)) != 0) {
			return ret;
		}
	} else {
		if ((ret = _tcp_send(handle, buf, 8, TCP_DEFAULT_TIMEOUT)) != 0) {
			return ret;
		}
		if ((ret = _tcp_send(handle, user, strlen(user) + 1, TCP_DEFAULT_TIMEOUT)) != 0) {
			return ret;
		}
	}

	if ((ret = _tcp_recv(handle, buf, (int *)8, 15000)) != 0) {
		return ret;
	}
	if (buf[1] == 0x5A) {
		return 0;
	} else if (buf[1] == 0x5D) {
		return ERR_TR50_SOCK_PROXY_AUTH_REJECTED;
	} else {
		return ERR_TR50_SOCK_PROXY_OTHER;
	}
}

int _socket_proxy_setup_socks4a(void *handle, const char *addr, const char *user) {
	char buf[9];
	struct sockaddr_in sin;
	int ret;
	unsigned short i_port;
	char host[256], *colon;

	strncpy(host, addr, 255);
	if ((colon = strchr(host, ':')) != NULL) {
		*colon = '\0';
	}

	if ((ret = make_addr(addr, &sin)) != 0) {
		return ret;
	}
	i_port = sin.sin_port;

	buf[0] = 0x04;
	buf[1] = 0x01;
	_memory_memcpy(buf + 2, &i_port, sizeof(short));
	buf[4] = 0x00;
	buf[5] = 0x00;
	buf[6] = 0x00;
	buf[7] = 0x03;
	buf[8] = 0x00;

	if (user == NULL) {
		if ((ret = _tcp_send(handle, buf, 9, TCP_DEFAULT_TIMEOUT)) != 0) {
			return ret;
		}
		if ((ret = _tcp_send(handle, host, strlen(host) + 1, TCP_DEFAULT_TIMEOUT)) != 0) {
			return ret;
		}
	} else {
		if ((ret = _tcp_send(handle, buf, 8, TCP_DEFAULT_TIMEOUT)) != 0) {
			return ret;
		}
		if ((ret = _tcp_send(handle, user, strlen(user) + 1, TCP_DEFAULT_TIMEOUT)) != 0) {
			return ret;
		}
		if ((ret = _tcp_send(handle, host, strlen(host) + 1, TCP_DEFAULT_TIMEOUT)) != 0) {
			return ret;
		}
	}

	if ((ret = _tcp_recv(handle, buf, (int *)8, 5000)) != 0) {
		return ret;
	}
	if (buf[1] == 0x5A) {
		return 0;
	} else if (buf[1] == 0x5D) {
		return ERR_TR50_SOCK_PROXY_AUTH_REJECTED;
	} else {
		return ERR_TR50_SOCK_PROXY_OTHER;
	}
}

int _socket_proxy_setup_socks5(void *handle, const char *addr, const char *user, const char *pass) {
	char buf[512];
	int buf_idx;
	struct sockaddr_in sin;
	int ret;
	unsigned short i_port;
	unsigned int   i_addr;
	char host[256], *colon;

	strncpy(host, addr, 255);
	if ((colon = strchr(host, ':')) != NULL) {
		*colon = '\0';
	}

	if ((ret = make_addr(addr, &sin)) != 0) {
		return ret;
	}

	i_addr = (unsigned int)sin.sin_addr.S_un.S_addr;

	i_port = sin.sin_port;

	buf[0] = 0x05;
	buf[1] = 0x02;
	buf[2] = 0x02;
	buf[3] = 0x00;

	if ((ret = _tcp_send(handle, buf, 4, TCP_DEFAULT_TIMEOUT)) != 0) {
		return ret;
	}
	if ((ret = _tcp_recv(handle, buf, (int *)2, 5000)) != 0) {
		return ret;
	}
	if ((unsigned char)(buf[1]) == 0xFF) {
		return ERR_TR50_SOCK_PROXY_AUTH_UNSUP;
	}

	if (buf[1] == 0x02) {
		buf_idx = 0;

		buf[buf_idx] = 0x01;
		buf_idx++;
		if (user != NULL) {
			buf[buf_idx] = strlen(user);
			strncpy(buf + buf_idx + 1, user, 256 - buf_idx);
			buf_idx += buf[buf_idx] + 1;
		} else {
			buf[buf_idx] = 0;
			buf_idx++;
		}
		if (pass != NULL) {
			buf[buf_idx] = strlen(pass);
			strncpy(buf + buf_idx + 1, pass, 256 - buf_idx);
			buf_idx += buf[buf_idx] + 1;
		} else {
			buf[buf_idx] = 0;
			buf_idx++;
		}

		if ((ret = _tcp_send(handle, buf, buf_idx, TCP_DEFAULT_TIMEOUT)) != 0) {
			return ret;
		}
		if ((ret = _tcp_recv(handle, buf, (int *)2, 5000)) != 0) {
			return ret;
		}
		if (buf[1] != 0) {
			return ERR_TR50_SOCK_PROXY_AUTH_REJECTED;
		}
	}

	buf[0] = 0x05;
	buf[1] = 0x01;
	buf[2] = 0x00;
	buf_idx = 3;

	if (_socket_proxy_uses_hostname(addr)) {
		char host[256], *colon;
		buf[buf_idx] = 0x03;
		buf_idx++;

		strncpy(host, addr, 255);
		if ((colon = strchr(host, ':')) != NULL) {
			*colon = '\0';
		}
		buf[buf_idx] = strlen(host);
		strncpy(buf + buf_idx + 1, host, 252);
		buf_idx += buf[buf_idx] + 1;
	} else {
		buf[buf_idx] = 0x01;
		buf_idx++;

		_memory_memcpy(buf + buf_idx, &i_addr, sizeof(int));
		buf_idx += 4;

	}

	_memory_memcpy(buf + buf_idx, &i_port, sizeof(short));
	buf_idx += 2;

	if ((ret = _tcp_send(handle, buf, buf_idx, TCP_DEFAULT_TIMEOUT)) != 0) {
		return ret;
	}
	if ((ret = _tcp_recv(handle, buf, (int *)6, 5000)) != 0) {
		return ret;
	}
	if (buf[1] == 0x00) {
		if (buf[3] == 0x01) {
			if ((ret = _tcp_recv(handle, buf, (int *)4, 5000)) != 0) {
				return ret;
			}
		} else if (buf[3] == 0x03) {
			if ((ret = _tcp_recv(handle, buf, (int *)1, 5000)) != 0) {
				return ret;
			}
			if ((ret = _tcp_recv(handle, buf, (int *)buf[4], 5000)) != 0) {
				return ret;
			}
		}
		return 0;
	} else {
		return ERR_TR50_SOCK_PROXY_OTHER;
	}
}

int _socket_proxy_setup_http(void *handle, const char *addr, const char *host, const char *user, const char *pass) {
	void *blob;
	char buffer;
	int ret;
	int termchr = 0;
	const char *host_colon;
	int status = 0;

	if ((ret = _blob_create(&blob, 128)) != 0) {
		return ret;
	}

	_blob_format_append(blob, "CONNECT ", NULL);

	_blob_format_append(blob, addr, NULL);

	_blob_format_append(blob, " HTTP/1.1\r\n", NULL);
	if ((host_colon = strchr(host, ':')) == NULL) {
		_blob_format_append(blob, "Host: %s\r\n", host);
	} else {
		_blob_format_append(blob, "Host: ", NULL);
		_blob_format_append(blob, host, host_colon - host);
		_blob_format_append(blob, "\r\n", NULL);
	}
	if (user != NULL && pass != NULL) {
		char *userpass, *userpass_e;
		int   userpass_len;

		userpass_len = strlen(user) + strlen(pass) + 8;
		if ((userpass = (char *)_memory_malloc(userpass_len)) == NULL) {
			_blob_delete(blob);
			return ERR_TR50_MALLOC;
		}
		_snprintf(userpass, userpass_len - 1, "%s:%s", user, pass);
		if ((ret = base64_encode((const unsigned char *)userpass, strlen(userpass), &userpass_e)) != 0) {
			_blob_delete(blob);
			_memory_free(userpass);
			return ret;
		}

		_blob_format_append(blob, "Authorization: Basic %s\r\n", userpass_e);

		_memory_free(userpass);
		_memory_free(userpass_e);
	}
	_blob_format_append(blob, "\r\n", NULL);

	if ((ret = _tcp_send(handle, (const char *)_blob_get_buffer(blob), _blob_get_length(blob), TCP_DEFAULT_TIMEOUT)) != 0) {
		return ret;
	}

	_blob_clear(blob);

	while (1) {

		buffer = 0;
		if ((ret = _tcp_recv(handle, &buffer, (int *)1, 5000)) != 0) {
			if (socket_error(handle) == ERR_TR50_SOCK_TIMEOUT) {
				continue;
			}
			break;
		} else {
			_blob_format_append(blob, &buffer, 1);
			if (buffer == '\r' || buffer == '\n') {
				++termchr;
				if (termchr == 4) {
					_blob_format_append(blob, '\0', 1);

					//have a complete response
					status = atoi(((char *)_blob_get_buffer(blob)) + 9);
					break;
				}
			} else {
				termchr = 0;
			}
		}
	}

	_blob_delete(blob);
	if (status == 200) {
		return 0;
	} else {
		return ERR_TR50_SOCK_PROXY_OTHER;
	}
}
