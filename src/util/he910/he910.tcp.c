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

#include <ipdata.h>
#include <netdb.h>
#include <inet.h>
#include <sockets.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys_arch.h>

#include <tr50/error.h>
#include <tr50/util/log.h>
#include <tr50/util/tcp.h>
#include <tr50/util/memory.h>
#include <tr50/util/time.h>

#include <utat_ipgprs.h>
#include <securesocket.h>
#include <ps/ip/ipssl.h>

#include "al/ba/nvm/nvmpst.h"
#include "at/atparser.h"
#include "at/cmdh/atsocket.h"

#define ERR_PARMS					-201
#define ERR_SOCK_SELECT_FAILED		-202
#define ERR_SOCK_TIMEOUT			ERR_TR50_TCP_TIMEOUT
#define ERR_SOCK_OTHER				-204
#define ERR_BADHANDLE				-205
#define ERR_SOCK_SEND_FAILED		-206
#define ERR_SOCK_CONNECT_FAILED		-207
#define ERR_SOCK_BAD_TIMEOUT		-208
#define ERR_SOCK_RECV_FAILED		-209
#define ERR_SOCK_SHUTDOWN			-210
#define ERR_SOCK_SOCKET_FAILED		-211
#define ERR_SOCK_SETSOCKOPT_FAILED	-212
#define ERR_SOCK_HOSTNOTFOUND		-213
#define ERR_SOCK_FNCTL_FAILED		-214


#define DTCPBUF	65536
#define GETNETERR	get_errno

#define accept(a,b,c)         lwip_accept(a,b,c)
#define bind(a,b,c)           lwip_bind(a,b,c)
#define shutdown(a,b)         lwip_shutdown(a,b)
#define close(s)              lwip_close(s)
#define connect(a,b,c)        lwip_connect(a,b,c)
#define getsockname(a,b,c)    lwip_getsockname(a,b,c)
#define getpeername(a,b,c)    lwip_getpeername(a,b,c)
#define setsockopt(a,b,c,d,e) lwip_setsockopt(a,b,c,d,e)
#define getsockopt(a,b,c,d,e) lwip_getsockopt(a,b,c,d,e)
#define listen(a,b)           lwip_listen(a,b)
#define recv(a,b,c,d)         lwip_recv(a,b,c,d)
#define read(a,b,c)           lwip_read(a,b,c)
#define recvfrom(a,b,c,d,e,f) lwip_recvfrom(a,b,c,d,e,f)
#define send(a,b,c,d)         lwip_send(a,b,c,d)
#define sendto(a,b,c,d,e,f)   lwip_sendto(a,b,c,d,e,f)
#define socket(a,b,c)         lwip_socket(a,b,c)
#define write(a,b,c)          lwip_write(a,b,c)
#if defined (DUAL_IP)
#define select(a,b,c,d,e)     lwip_select_ex(a,b,c,d,e)
#else
#define select(a,b,c,d,e)     lwip_select_ex(a,b,c,d,e,0)
#endif

typedef struct {
	int s;
	int err;
	int options;
	SECURE_SOCKET ssid;
} _TCP_OBJECT;

int _tcp_secure_connect(_TCP_OBJECT *sock);
int _tcp_secure_send(_TCP_OBJECT *sock, const char *buffer, int length, int timeout);
int _tcp_secure_recv(_TCP_OBJECT *sock, char *buffer, int *length, int timeout);
int _tcp_secure_disconnect(_TCP_OBJECT *sock);


int host2addr(const char *host, struct in_addr *addr) {
	struct hostent *hoste;
	unsigned int index;

	index = Cid2Index(1);

	if (index == 0xFF) {
		return ERR_SOCK_HOSTNOTFOUND;
	}
	SYprintf("host2addr ---------------- host = %s\n", host);
	if ((hoste = gethostbyname(IPIndex[index].netid, host)) == NULL) {
		return ERR_SOCK_HOSTNOTFOUND;
	}

	if (addr == NULL) {
		return ERR_SOCK_HOSTNOTFOUND;
	}
	if (hoste->h_addr_list[0] == NULL) {
		return ERR_SOCK_HOSTNOTFOUND;
	}

	_memory_memcpy(addr, hoste->h_addr_list[0], hoste->h_length);

	return 0;
}

int make_addr(const char *addr, struct sockaddr_in *sin) {
	char *colon;
	if ((addr == NULL) || (sin == NULL)) {
		return ERR_PARMS;
	}
	_memory_memset(sin, 0, sizeof(struct sockaddr_in));
	sin->sin_family = AF_INET;

	colon = strchr(addr, ':');
	if (colon != NULL) {
		*colon = '\0';
	}

	if (host2addr(addr, &(sin->sin_addr)) != 0) {
		return ERR_SOCK_HOSTNOTFOUND;
	}

	if (colon != NULL) {
		sin->sin_port = htons(atoi(colon + 1));
		*colon = ':';
	} else {
		sin->sin_port = 0;
	}
	return 0;
}

static void set_frwl(BOOLEAN set, UINT32 ip_addr) {
	FRWL_TYPE *Msg = SY_FASTNEW(FRWL_TYPE);
	static UINT32 ServerIPAddress = 0;

#if (defined(IPV6_TCP))
	Msg->IPAddress[0] = 0;

	if (!set && ServerIPAddress) { //clear frwl and ip address previously set
		Msg->IPAddress[0] = ServerIPAddress;
	} else if (ip_addr) { //ip address different from 0
		ServerIPAddress = Msg->IPAddress[0] = ip_addr;
	}

	if (Msg->IPAddress[0]) {
		Msg->IPMask[0] = 0xFFFFFFFFL;
		Msg->Set = set;
		Msg->PDPType = PDP_IPV4;
		SYSendMessage(ATSAP, BSENSE_FRWL_SET, Msg);
	}
#else
	Msg->IPAddress = 0;

	if (!set && ServerIPAddress) { //clear frwl and ip address previously set
		Msg->IPAddress = ServerIPAddress;
	} else if (ip_addr) { //ip address different from 0
		ServerIPAddress = Msg->IPAddress = ip_addr;
	}

	if (Msg->IPAddress) {
		Msg->IPMask = 0xFFFFFFFFL;
		Msg->Set = set;
		SYSendMessage(ATSAP, BSENSE_FRWL_SET, Msg);
	}
#endif
}

int _tcp_connect(void **handle, const char *addr, long port, int options) {
	_TCP_OBJECT *sock = (_TCP_OBJECT *)_memory_malloc(sizeof(_TCP_OBJECT));
	struct linger	llinger;
	int		tint;
	struct sockaddr_in sa;
	int ret;
	int count = 0;
	unsigned int index;

	if (handle == NULL) {
		return ERR_BADHANDLE;
	}

	_memory_memset(sock, 0, sizeof(_TCP_OBJECT));
	sock->options = options;

	llinger.l_onoff = 1;
	llinger.l_linger = 0;

	while ((sock->s = (int)socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0 && count++ < 10);
	if (count >= 10) {
		sock->err = GETNETERR();
		return ERR_SOCK_SOCKET_FAILED;
	}
	tint = 1;

	index = Cid2Index(1);
	if (index == 0xFF) {
		return ERR_SOCK_HOSTNOTFOUND;
	}

	lwip_socket_set_netid(sock->s, (unsigned char)IPIndex[index].netid);

	if (setsockopt(sock->s, SOL_SOCKET, SO_KEEPALIVE, (char *)&tint, sizeof(tint)) != 0) {
		ret = ERR_SOCK_SETSOCKOPT_FAILED;
		goto end_error;
	}

	if (setsockopt(sock->s, SOL_SOCKET, SO_OOBINLINE, (char *)&tint, sizeof(tint)) != 0) {
		sock->err = GETNETERR();
	}

	if (setsockopt(sock->s, SOL_SOCKET, SO_LINGER, (char *)&llinger, sizeof(llinger)) != 0) {
		ret = ERR_SOCK_SETSOCKOPT_FAILED;
		goto end_error;
	}

	if ((options & TCP_DONT_SET_QUEUE_SIZES) == TCP_DONT_SET_QUEUE_SIZES) {
		tint = DTCPBUF;
		if (setsockopt(sock->s, SOL_SOCKET, SO_SNDBUF, (char *)&tint, sizeof(tint)) != 0) {
			ret = ERR_SOCK_SETSOCKOPT_FAILED;
			goto end_error;
		}

		if (setsockopt(sock->s, SOL_SOCKET, SO_RCVBUF, (char *)&tint, sizeof(tint)) != 0) {
			ret = ERR_SOCK_SETSOCKOPT_FAILED;
			goto end_error;
		}
	}
	{
		char address[128];
		snprintf(address, 128, "%s:%d\0", addr, port);
		if ((ret = make_addr(address, &sa)) != 0) {
			goto end_error;
		}
	}
	//sock->peer_addr=ntohl(((unsigned int)sa.sin_addr.s_addr));
	set_frwl(TRUE, sa.sin_addr.s_addr);
	//sock->peer_port=ntohs(sa.sin_port);
	if ((ret = connect(sock->s, (struct sockaddr *)&sa, sizeof(sa))) < 0) {
		ret = ERR_SOCK_CONNECT_FAILED;
		goto end_error;

	}

	if (options & TCP_OPTION_SECURE) {
		ret = _tcp_secure_connect(sock);
	}

	if (ret == 0) {
		*handle = (void *)sock;
		//sock->connect_time=_time_now();
		return 0;
	} else {
		*handle = NULL;
		ret = ERR_SOCK_CONNECT_FAILED;
		goto end_error;
	}
end_error:
	sock->err = GETNETERR();
	close(sock->s);
	_memory_free(sock);
	return ret;
}
int _tcp_connect_proxy(void **sock, const char *addr, long port, int options, int proxy_type, const char *proxy_addr, const char *proxy_username, const char *proxy_password) {
	return ERR_TR50_NOPORT;
}

int _tcp_ssl_config(const char *password, const char *file, int verify_peer) {
	return ERR_TR50_NOPORT;
}

int _tcp_disconnect(void *socket) {
	_TCP_OBJECT *tcp = (_TCP_OBJECT *)socket;

	if (tcp->options & TCP_OPTION_SECURE) {
		_tcp_secure_disconnect(tcp);
	}

	shutdown(tcp->s, 2);
	close(tcp->s);
	set_frwl(FALSE, 0);
	_memory_free(tcp);
	return 0;
}

int _tcp_send(void *handle, const char *buffer, int length, int timeout) {
	_TCP_OBJECT *sock = handle;
	fd_set sockSet;
	int current_sent = 0;
	struct timeval to;
	int ret;

	if (handle == NULL) {
		return ERR_BADHANDLE;
	}

	if (sock->options & TCP_OPTION_SECURE) {
		return _tcp_secure_send(sock, buffer, length, timeout);
	}
	if (length == -1) {
		length = strlen(buffer) + 1;
	}

	if (timeout < -1) {
		sock->err = GETNETERR();
		return ERR_SOCK_OTHER;
	} else {
		to.tv_usec = (timeout % 1000) * 1000;
		to.tv_sec = timeout / 1000;
	}

	while (1) {

		if (timeout > 0) {
			FD_ZERO(&sockSet);
			FD_SET(sock->s, &sockSet);
			if ((ret = select(sock->s + 1, NULL, &sockSet, NULL, &to)) == 0) {
				sock->err = ERR_SOCK_TIMEOUT;
				return ERR_SOCK_TIMEOUT;
			} else if (ret == -1) {
				sock->err = GETNETERR();
				return ERR_SOCK_SELECT_FAILED;
			}
		}

		if (length - current_sent > DTCPBUF) {
			ret = send(sock->s, (void *)(buffer + current_sent), DTCPBUF, 0);
		} else {
			ret = send(sock->s, (void *)(buffer + current_sent), length - current_sent, 0);
		}

		if ((ret + current_sent) == length) {
			return 0;
		} else if (ret == -1) {
			sock->err = GETNETERR();
			return ERR_SOCK_SEND_FAILED;
		} else {
			current_sent += ret;
		}
	}
}

int _tcp_recv(void *handle, char *buffer, int *length, int timeout) {
	_TCP_OBJECT *sock = handle;
	fd_set	sockSet;
	struct timeval to;
	int ret;

	if (handle == NULL) {
		return ERR_BADHANDLE;
	}

	if (sock->options & TCP_OPTION_SECURE) {
		return _tcp_secure_recv(sock, buffer, length, timeout);
	}

	if (timeout < -1) {
		sock->err = GETNETERR();
		return ERR_SOCK_BAD_TIMEOUT;
	}

	if (timeout > 0) {
		to.tv_usec = (timeout % 1000) * 1000;
		to.tv_sec = timeout / 1000;

		FD_ZERO(&sockSet);
		FD_SET(sock->s, &sockSet);
		if ((ret = select(sock->s + 1, &sockSet, NULL, NULL, &to)) == 0) {
			sock->err = ERR_SOCK_TIMEOUT;
			return ERR_SOCK_TIMEOUT;
		} else if (ret == -1) {
			sock->err = GETNETERR();
			return ERR_SOCK_SELECT_FAILED;
		}
	}

	ret = recv(sock->s, buffer, *length, 0);

	if (ret == -1) {
		sock->err = GETNETERR();
		*length = 0;
		return ERR_SOCK_RECV_FAILED;
	} else if (ret == 0) {
		sock->err = ERR_SOCK_SHUTDOWN;
		*length = 0;
		return ERR_SOCK_SHUTDOWN;
	} else {
		*length = ret;
		return 0;
	}
}

int g_tcp_secure_init_run_once = 0;
int _tcp_secure_init() {
	int trust_ca_add_check = 0;
	int certSize, certStart, selectedPst, certIsPem, derCertSize, ret;
	char IPSSLTempBuffer[4001 + 4], IPSSLDerTempBuffer[4001 + 4];
	SSL_STORAGE_TYPE *pSSLConfig = &IPSSLConfigMulti[0];
	SSL_INFO_TYPE *ptrSSLConn = &SSLConn[0];

	log_debug("_tcp_secure_init(): ...");
	SYmemset(IPSSLTempBuffer, 0, sizeof(IPSSLTempBuffer));
	SYmemset(IPSSLDerTempBuffer, 0, sizeof(IPSSLDerTempBuffer));

	selectedPst = SSLSecDataCACert1;

	if ((pSSLConfig->SSLSecLevel <= IPSSL_SecLev_NoAuth) || !(ptrSSLConn->SecDataSet & SSL_CACERT_MASK)) {
		g_tcp_secure_init_run_once = 1;
		log_debug("_tcp_secure_init(): no auth");
		return 0;
	}

	log_debug("_tcp_secure_init(): server auth");
	if (NVMRead(SSLSecData, selectedPst, IPSSLTempBuffer) != ERR_NONE) {
		// ... error while reading CA cert from NVM ...
		return ERR_SOCK_CONNECT_FAILED;
	}

	certSize = ((SSL_CACERT_TYPE *)IPSSLTempBuffer)->CACertSize;
	certStart = sizeof(((SSL_CACERT_TYPE *)IPSSLTempBuffer)->CACertSize);

	certIsPem = certFormatIsPem(&IPSSLTempBuffer[certStart]);

	if (((!IPSSLConfigMulti[0].SSLCertFormat) && certIsPem) ||
			((IPSSLConfigMulti[0].SSLCertFormat) && (!certIsPem))) {
		// ... error in cert format	...
		return ERR_SOCK_CONNECT_FAILED;
	}

	if (IPSSLConfigMulti[0].SSLCertFormat) {

		log_debug("_tcp_secure_init(): trust ca add enh PEM format");

		if ((derCertSize = certFormatPemToDer(&IPSSLTempBuffer[certStart], IPSSLDerTempBuffer)) < 0) {
			// ... error while converting from PEM to DER format ...
		}
		trust_ca_add_check = trust_ca_add_enh(IPSSLDerTempBuffer, derCertSize);
	} else {
		log_debug("_tcp_secure_init(): trust ca add enh DER format");
		trust_ca_add_check = trust_ca_add_enh(&IPSSLTempBuffer[certStart], certSize);
	}

	if (trust_ca_add_check != 0) {
		// ... error while loading trust CA cert into RAM ...
		return ERR_SOCK_CONNECT_FAILED;
	}
	g_tcp_secure_init_run_once = 1;
	return 0;
}

int _tcp_secure_connect(_TCP_OBJECT *sock) {
	ConnectionData secSockData;
	SECURE_SOCKET ssid;
	int ret;
	SSL_STORAGE_TYPE *pSSLConfig = &IPSSLConfigMulti[0];
	SSL_INFO_TYPE *ptrSSLConn = &SSLConn[0];

	log_debug("_tcp_secure_connect(): ...");
	if (!g_tcp_secure_init_run_once && _tcp_secure_init() != 0) {
		return ERR_SOCK_CONNECT_FAILED;
	}
	log_debug("_tcp_secure_connect(): init completed");

	SYmemset(&secSockData, 0x00, sizeof(ConnectionData));
	secSockData.client_certfile = NULL;
	secSockData.client_keyfile = NULL;
	secSockData.method = (ClientMethod)ClientMethodTLSV1;

	if ((ssid = securesocket(sock->s, &secSockData)) < 0) {
		// ... error while creating secure socket from TCP socket ...
		log_need_investigation("_tcp_secure_connect(): securesocket failed [%d]", ssid);
		return ERR_SOCK_CONNECT_FAILED;
	}
	log_debug("_tcp_secure_connect(): securesocket OK");

	if ((pSSLConfig->SSLSecLevel <= IPSSL_SecLev_NoAuth) || !(ptrSSLConn->SecDataSet & SSL_CACERT_MASK)) {
		securesocket_set_verify_method(ssid, 0);
	}

	log_debug("_tcp_secure_connect(): connecting ...");
	if ((ret = securesocket_connect(ssid)) != 0) {
		// ... error during TLS handshake ...
		securesocket_shutdown(ssid);
		securesocket_close(ssid);
		log_debug("_tcp_secure_connect(): securesocket_connect failed [%d]!", ret);
		return ERR_SOCK_CONNECT_FAILED;
	}
	log_debug("_tcp_secure_connect(): connected!");
	securesocket_get_ciphersuite(ssid);

	sock->ssid = ssid;
	return 0;
}

int _tcp_secure_send(_TCP_OBJECT *sock, const char *buffer, int length, int timeout) {
	fd_set sockSet;
	int current_sent = 0;
	struct timeval to;
	int ret;

	if (sock == NULL) {
		return ERR_BADHANDLE;
	}

	if (length == -1) {
		length = strlen(buffer) + 1;
	}

	if (timeout < -1) {
		sock->err = GETNETERR();
		return ERR_SOCK_OTHER;
	} else {
		to.tv_usec = (timeout % 1000) * 1000;
		to.tv_sec = timeout / 1000;
	}

	while (1) {
		if (length - current_sent > DTCPBUF) {
			ret = securesocket_send(sock->ssid, (const void *)(buffer + current_sent), DTCPBUF, 0);
		} else {
			log_hexdump(LOG_TYPE_LOW_LEVEL, "_tcp_secure_sending(): sending", buffer + current_sent, length - current_sent);
			ret = securesocket_send(sock->ssid, (const void *)(buffer + current_sent), length - current_sent, 0);
			log_debug("_tcp_secure_sending(): returns [%d]", ret);
		}

		if ((ret + current_sent) == length) {
			return 0;
		} else if (ret == -1) {
			sock->err = GETNETERR();
			return ERR_SOCK_SEND_FAILED;
		} else {
			current_sent += ret;
		}
	}
}

int _tcp_has_been_closed(int s) {
	unsigned int optval = 0;
	int optlen = sizeof(unsigned int);

	if (!getsockopt(s, (int)SOL_SOCKET, (int)SO_CONNCLOSED, (char *)&optval, &optlen)) {
		return optval;
	} else {
		return 0;
	}
}

int _tcp_secure_recv(_TCP_OBJECT *sock, char *buffer, int *length, int timeout) {
	int ret;

	if (setsockopt(sock->s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(unsigned int)) != 0) {
		sock->err = GETNETERR();
	}
	log_debug("_tcp_secure_recv(): ...");
	if ((ret = securesocket_recv(sock->ssid, (void *)buffer, *length, 0)) > 0) {
		log_debug("_tcp_secure_recv(): securesocket_recv returns [%d]", ret);
		*length = ret;
		return 0;
	}


	if ((ret == SecureSocketErrSysCall) && _tcp_has_been_closed(sock->s)) {
		log_debug("_tcp_secure_recv(): securesocket_recv returned [SecureSocketErrSysCall] ");
		return ERR_SOCK_RECV_FAILED;
	}


	if (ret == SecureSocketErrZeroReturn) {
		log_debug("_tcp_secure_recv(): securesocket_recv returned [SecureSocketErrZeroReturn]");
		return ERR_SOCK_RECV_FAILED;
	}
	return ERR_SOCK_TIMEOUT;
}

int _tcp_secure_disconnect(_TCP_OBJECT *sock) {
	log_debug("_tcp_secure_disconnect(): ...");
	securesocket_shutdown(sock->ssid);
	securesocket_close(sock->ssid);
	log_debug("_tcp_secure_disconnect(): OK");
	return 0;
}

