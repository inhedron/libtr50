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

#include <stdio.h>

#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include <tr50/error.h>

#include <tr50/util/blob.h>
#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/mutex.h>
#include <tr50/util/tcp.h>
#include <tr50/util/time.h>
#include <tr50/util/thread.h>

unsigned long _id_function(void);
void _dyn_lock_function(int mode, struct CRYPTO_dynlock_value *l, const char *file, int line);
void _dyn_destroy_function(struct CRYPTO_dynlock_value *l, const char *file, int line);
struct CRYPTO_dynlock_value *_dyn_create_function(const char *file, int line);
void _locking_function(int mode, int n, const char *file, int line);

#define SSL_ERR_STR_LEN		512
#define SSL_PASSWD_LEN		64
#define SSL_BUFF			64512

typedef struct {
	int s;
	int type;
	int err;
	int is_ssl;
	void *sslo;
} ABSTRACT_SOCKET;

typedef struct {
	SSL *ssl;
	int errcode;
	char errmsg[SSL_ERR_STR_LEN + 1];
} _SSLO;

typedef struct {
	SSL_CTX *ctx;
	char passwd[SSL_PASSWD_LEN + 1];
	int errcode;
	char errmsg[SSL_ERR_STR_LEN + 1];
} _SSLO_CTX;

struct CRYPTO_dynlock_value {
	void *mux;
};

void **g_ssl_locks = NULL;

void _ssl_init() {
	// check if ssl already initialized
	int num_locks;
	int i;
	if (g_ssl_locks != NULL) {
		return;
	}
	num_locks = CRYPTO_num_locks();
	g_ssl_locks = _memory_malloc(sizeof(int *)*num_locks);
	if (g_ssl_locks == NULL) {
		return;
	}

	for (i = 0; i < num_locks; ++i) {
		_tr50_mutex_create(&(g_ssl_locks[i]));
	}

	CRYPTO_set_id_callback(_id_function);
	CRYPTO_set_locking_callback(_locking_function);
	CRYPTO_set_dynlock_create_callback(_dyn_create_function);
	CRYPTO_set_dynlock_lock_callback(_dyn_lock_function);
	CRYPTO_set_dynlock_destroy_callback(_dyn_destroy_function);

	SSL_library_init();
	SSL_load_error_strings();
	//random_init();
	return;
}

void _locking_function(int mode, int n, const char *file, int line) {
	if (mode & CRYPTO_LOCK) {
		_tr50_mutex_lock(g_ssl_locks[n]);
	} else {
		_tr50_mutex_unlock(g_ssl_locks[n]);
	}
}

unsigned long _id_function(void) {
	int id=0;
	_thread_id(&id);
	return (unsigned long)id;
}

struct CRYPTO_dynlock_value *_dyn_create_function(const char *file, int line) {
	struct CRYPTO_dynlock_value *value = NULL;
	_tr50_mutex_create((void **) & (value->mux));
	return value;
}

void _dyn_lock_function(int mode, struct CRYPTO_dynlock_value *l, const char *file, int line) {
	if (mode & CRYPTO_LOCK) {
		_tr50_mutex_lock(l);
	} else {
		_tr50_mutex_unlock(l);
	}
	return;
}

void _dyn_destroy_function(struct CRYPTO_dynlock_value *l, const char *file, int line) {
	_tr50_mutex_delete(l);
}

int __ssl_morph(void *handle) {
	ABSTRACT_SOCKET *so = handle;
	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}
	so->is_ssl = 1;
	return 0;
}

int __ssl_send2(void *handle, const char *buffer, int length, int timeout) {
	ABSTRACT_SOCKET *so = handle;
	_SSLO *sslo = so->sslo;
	int ret;
	int current_sent = 0;
	fd_set	sockSet;
	struct timeval to;

	if (sslo == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (timeout < -1) {
		//so->err = GETNETERR();
		return ERR_TR50_SOCK_OTHER;
	} else {
		to.tv_usec = (timeout % 1000) * 1000;
		to.tv_sec = timeout / 1000;
	}

	while (1) {
		if (timeout > 0) {
			FD_ZERO(&sockSet);
			FD_SET(so->s, &sockSet);
			if ((ret = select(so->s + 1, NULL, &sockSet, NULL, &to)) == 0) {
				so->err = ERR_TR50_SOCK_TIMEOUT;
				return ERR_TR50_SOCK_TIMEOUT;
			} else if (ret == -1) {
				//so->err = GETNETERR();
				return ERR_TR50_SOCK_SELECT_FAILED;
			}
		}

		if (length - current_sent > SSL_BUFF) {
			ret = SSL_write(sslo->ssl, buffer + current_sent, SSL_BUFF);
		} else {
			ret = SSL_write(sslo->ssl, buffer + current_sent, length - current_sent);
		}
		if ((ret + current_sent) == length) {
			return 0;
		} else if (ret == -1) {
			sslo->errcode = SSL_get_error(sslo->ssl, ret);
			if (sslo->errcode == 0x1409F07F) {
				_thread_sleep(10);
				continue;
			}
			//ERR_error_string_n(sslo->errcode, sslo->errmsg, SSL_ERR_STR_LEN);
			return ERR_TR50_SSL_GENERIC;
		} else {
			current_sent += ret;
		}
	}
}

int __ssl_recv2(void *handle, char *buffer, int *length, int timeout) {
	ABSTRACT_SOCKET *so = handle;
	_SSLO *sslo = so->sslo;
	int ret;
	struct timeval to;
	fd_set	sockSet;

	if (sslo == NULL) {
		return ERR_TR50_BADHANDLE;
	}
	if (timeout < -1) {
		//so->err = GETNETERR();
		return ERR_TR50_SOCK_OTHER;
	}

	if (SSL_pending(sslo->ssl) == 0 && timeout > 0) {
		to.tv_usec = (timeout % 1000) * 1000;
		to.tv_sec = timeout / 1000;

		FD_ZERO(&sockSet);
		FD_SET(so->s, &sockSet);
		if ((ret = select(so->s + 1, &sockSet, NULL, NULL, &to)) == 0) {
			so->err = ERR_TR50_SOCK_TIMEOUT;
			return ERR_TR50_SOCK_TIMEOUT;
		} else if (ret == -1) {
			//so->err = GETNETERR();
			return ERR_TR50_SOCK_OTHER;
		}
	}

	ret = SSL_read(sslo->ssl, buffer, *length);
	if (ret == -1) {
		*length = 0;
		if ((SSL_get_shutdown(sslo->ssl) & SSL_RECEIVED_SHUTDOWN)) {
			so->err = ERR_TR50_SOCK_SHUTDOWN;
			return ERR_TR50_SOCK_SHUTDOWN;
		}
		sslo->errcode = SSL_get_error(sslo->ssl, ret);
		//ERR_error_string_n(sslo->errcode, sslo->errmsg, SSL_ERR_STR_LEN);
		return ERR_TR50_SSL_GENERIC;
	} else if (ret == 0) {
		*length = 0;
		so->err = ERR_TR50_SOCK_SHUTDOWN;
		return ERR_TR50_SOCK_SHUTDOWN;
	} else {
		*length = ret;
		log_hexdump(LOG_TYPE_LOW_LEVEL, "_ssl_recv2()", buffer, ret);
		return 0;
	}
}

int _ssl_close(void *handle) {
	ABSTRACT_SOCKET *so = handle;
	_SSLO *sslo = so->sslo;
	if (sslo == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	SSL_shutdown(sslo->ssl);
	SSL_free(sslo->ssl);
	_memory_free(sslo);
	return 0;
}

int _ssl_ctx_create(void **ctx) {
	_SSLO_CTX *so;
	if (ctx == NULL) {
		return ERR_TR50_PARMS;
	}
	*ctx = NULL;
	if ((so = _memory_malloc(sizeof(_SSLO_CTX))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(so, 0, sizeof(_SSLO_CTX));

	so->ctx = SSL_CTX_new(TLSv1_method());
	if (so->ctx == NULL) {
		_memory_free(so);
		return ERR_TR50_SSL_NOTINIT;
	}

	SSL_CTX_set_session_cache_mode(so->ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_mode(so->ctx, SSL_MODE_AUTO_RETRY);

	*ctx = so;
	return 0;
}

int _ssl_ctx_delete(void *ctx) {
	_SSLO_CTX *so = ctx;
	if (ctx == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	SSL_CTX_free(so->ctx);
	_memory_free(ctx);
	return 0;
}

int _ssl_ctx_set_passwd(void *ctx, const char *passwd) {
	_SSLO_CTX *so = ctx;
	if (ctx == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	strncpy(so->passwd, passwd, SSL_PASSWD_LEN);
	return 0;
}

const char *_ssl_error(void *handle) {
	ABSTRACT_SOCKET *so = handle;
	_SSLO *sslo;
	if (handle == NULL) {
		return NULL;
	}
	sslo = so->sslo;
	return sslo->errmsg;
}

const char *_ssl_ctx_error(void *ctx) {
	_SSLO_CTX *so = ctx;
	if (ctx == NULL) {
		return NULL;
	}
	return so->errmsg;
}

int _ssl_ctx_set_verify(void *ctx, int require_peer_certificate, int verify_peer) {
	_SSLO_CTX *so = ctx;
	int mask = 0;

	if (ctx == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (verify_peer) {
		mask |= SSL_VERIFY_PEER;
	}
	if (require_peer_certificate) {
		mask |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
	}
	SSL_CTX_set_verify(so->ctx, mask, NULL);
	return 0;
}

int _ssl_ctx_passwd_cb(char *buf, int size, int flag, void *passwd) {
	strncpy(buf, (char *)passwd, size);
	buf[size - 1] = '\0';
	return strlen(buf);
}

int _ssl_ctx_set_packed_file(void *ctx, const char *cfile) {
	_SSLO_CTX *so = ctx;
	if (ctx == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (SSL_CTX_use_certificate_chain_file(so->ctx, cfile) != 1) {
		//so->errcode = ERR_get_error();
		//ERR_error_string_n(so->errcode, so->errmsg, SSL_ERR_STR_LEN);
		return ERR_TR50_SSL_CTX;
	}

	if (SSL_CTX_load_verify_locations(so->ctx, cfile, NULL) != 1) {
		//so->errcode = ERR_get_error();
		//ERR_error_string_n(so->errcode, so->errmsg, SSL_ERR_STR_LEN);
		return ERR_TR50_SSL_CTX;
	}

	if (SSL_CTX_set_default_verify_paths(so->ctx) != 1) {
		//so->errcode = ERR_get_error();
		//ERR_error_string_n(so->errcode, so->errmsg, SSL_ERR_STR_LEN);
		return ERR_TR50_SSL_CTX;
	}

	SSL_CTX_set_default_passwd_cb(so->ctx, _ssl_ctx_passwd_cb);
	SSL_CTX_set_default_passwd_cb_userdata(so->ctx, so->passwd);
	if (SSL_CTX_use_PrivateKey_file(so->ctx, cfile, SSL_FILETYPE_PEM) != 1) {
		//so->errcode = ERR_get_error();
		//ERR_error_string_n(so->errcode, so->errmsg, SSL_ERR_STR_LEN);
		return ERR_TR50_SSL_CTX;
	}

	return 0;
}

int _ssl_connect(void *handle, void *ctx) {
	ABSTRACT_SOCKET *so = handle;
	int ret;
	_SSLO *sslo;
	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	ret = __ssl_morph(handle);
	if (ret != 0) {
		return ret;
	}

	sslo = so->sslo = _memory_malloc(sizeof(_SSLO));


	if (sslo == NULL) {
		return ERR_TR50_BADHANDLE;
	}
	_memory_memset(sslo, 0, sizeof(_SSLO));
	if (ctx == NULL) {
		return ERR_TR50_SSL_BADCTX;
	}

	if ((sslo->ssl = SSL_new(((_SSLO_CTX *)ctx)->ctx)) == NULL) {
		//sslo->errcode = ERR_get_error();
		//ERR_error_string_n(sslo->errcode, sslo->errmsg, SSL_ERR_STR_LEN);
		return ERR_TR50_SSL_CONNECT;
	}

	SSL_set_mode(sslo->ssl, SSL_MODE_AUTO_RETRY);
	SSL_set_fd(sslo->ssl, so->s);

	if ((ret = SSL_connect(sslo->ssl)) <= 0) {
		sslo->errcode = SSL_get_error(sslo->ssl, ret);
		//ERR_error_string_n(sslo->errcode, sslo->errmsg, SSL_ERR_STR_LEN);
		printf("%s", sslo->errmsg);
		ret = ERR_TR50_SSL_CONNECT;
	} else {
		ret = 0;
	}

	return ret;
}
