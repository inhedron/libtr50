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

#define TCP_DONT_SET_QUEUE_SIZES	1
#define TCP_USE_NODELAY				2
#define TCP_OPTION_SECURE			4

#define TCP_PROXY_TYPE_NONE			0
#define TCP_PROXY_TYPE_HTTP			1
#define TCP_PROXY_TYPE_SOCK4		2
#define TCP_PROXY_TYPE_SOCK4A		3
#define TCP_PROXY_TYPE_SOCK5		4

/* SSL setting is global */
int _tcp_ssl_config(const char *password, const char *file, int verify_peer);

int _tcp_connect_proxy(void **sock, const char *addr, long port, int options, int proxy_type, const char *proxy_addr, const char *proxy_username, const char *proxy_password);
int _tcp_connect(void **sock, const char *addr, long port, int options);
int _tcp_disconnect(void *sock);
int _tcp_send(void *sock, const char *buf, int len, int timeout);
int _tcp_recv(void *sock, char *buf, int *len, int timeout);
