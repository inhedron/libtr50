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

#include <string.h>
#include <time.h>

#include <tr50/mqtt/mqtt.h>

#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/platform.h>
#include <tr50/util/tcp.h>

typedef struct {
	char *host;
	long port;
	char client_id[24];
	char *username;
	char *password;
	unsigned short keepalive_in_sec;
	int use_ssl;
	int use_https_proxy;

	int		proxy_type;
	char 	*proxy_addr;
	char 	*proxy_username;
	char 	*proxy_password;
} _MQTT_COONNECT_PARAMS;

int _mqtt_https_connect(void *sock, _MQTT_COONNECT_PARAMS *params);

int mqtt_connect_params_create(void **connect_params, const char *client_id, const char *host, long port, unsigned short keepalive_in_sec) {
	_MQTT_COONNECT_PARAMS *params;

	if ((params = (_MQTT_COONNECT_PARAMS *)_memory_malloc(sizeof(_MQTT_COONNECT_PARAMS))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(params, 0, sizeof(_MQTT_COONNECT_PARAMS));
	strncpy(params->client_id, client_id, 23);
	params->host = (char *)_memory_clone((void *)host, strlen(host));
	params->port = port;
	params->keepalive_in_sec = keepalive_in_sec;
	*connect_params = params;
	return 0;
}

int mqtt_connect_params_set_username(void *connect_params, const char *username, const char *password) {
	_MQTT_COONNECT_PARAMS *params = (_MQTT_COONNECT_PARAMS *)connect_params;
	params->username = (char *)_memory_clone((void *)username, strlen(username));
	params->password = (char *)_memory_clone((void *)password, strlen(password));
	return 0;
}

int mqtt_connect_params_use_ssl(void *connect_params) {
	_MQTT_COONNECT_PARAMS *params = (_MQTT_COONNECT_PARAMS *)connect_params;
	params->use_ssl = 1;
	return 0;
}

int mqtt_connect_params_use_https_proxy(void *connect_params) {
	_MQTT_COONNECT_PARAMS *params = (_MQTT_COONNECT_PARAMS *)connect_params;
	params->use_ssl = 1;
	params->use_https_proxy = 1;
	return 0;
}

int mqtt_connect_params_use_proxy(void *connect_params, int type, const char *addr, const char *username, const char *password) {
	_MQTT_COONNECT_PARAMS *params = (_MQTT_COONNECT_PARAMS *)connect_params;
	params->proxy_type = type;
	if (addr) {
		params->proxy_addr = (char *)_memory_clone((void *)addr, strlen(addr));
	}
	if (username) {
		params->proxy_username = (char *)_memory_clone((void *)username, strlen(username));
	}
	if (password) {
		params->proxy_password = (char *)_memory_clone((void *)password, strlen(password));
	}
	return 0;
}

int mqtt_connect_params_delete(void *connect_params) {
	_MQTT_COONNECT_PARAMS *params = (_MQTT_COONNECT_PARAMS *)connect_params;
	if (params->host) {
		_memory_free(params->host);
	}
	if (params->username) {
		_memory_free(params->username);
	}
	if (params->password) {
		_memory_free(params->password);
	}
	_memory_free(params);
	return 0;
}

unsigned short mqtt_connect_params_get_keepalive(void *connect_params) {
	_MQTT_COONNECT_PARAMS *params = (_MQTT_COONNECT_PARAMS *)connect_params;
	return params->keepalive_in_sec;
}

int mqtt_connect(void **mqtt_handle, void *connect_params) {
	void *sock = NULL;
	int ret, req_len, rsp_len;
	char *req = NULL, *rsp = NULL;
	_MQTT_COONNECT_PARAMS *params = (_MQTT_COONNECT_PARAMS *)connect_params;
	_MQTT_CLIENT *client;
	int options = 0;

	if ((client = (_MQTT_CLIENT *)_memory_malloc(sizeof(_MQTT_CLIENT))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(client, 0, sizeof(_MQTT_CLIENT));

	if (params->use_ssl) {
		options |= TCP_OPTION_SECURE;
	}
	if (params->proxy_type > 0) {
		if ((ret = _tcp_connect_proxy(&sock,
									  params->host,
									  params->port,
									  options,
									  params->proxy_type,
									  params->proxy_addr,
									  params->proxy_username,
									  params->proxy_password)) != 0) {
			goto end_error;
		}
	} else {
		if ((ret = _tcp_connect(&sock, params->host, params->port, options)) != 0) {
			goto end_error;
		}
	}
	client->sock = sock;

	if (params->use_https_proxy) {
		if ((ret = _mqtt_https_connect(sock, params)) != 0) {
			goto end_error;
		}
	}

	if ((ret = _mqtt_msg_build_connect(params->client_id, params->username, params->password, params->keepalive_in_sec, &req, &req_len)) != 0) {
		goto end_error;
	}

	if ((ret = mqtt_send(client, req, req_len, 5000)) != 0) {
		goto end_error;
	}

	if ((ret = mqtt_recv(client, &rsp, &rsp_len, 5000)) != 0) {
		goto end_error;
	}

	if ((ret = mqtt_msg_process_connack(rsp, rsp_len)) != 0) {
		goto end_error;
	}

	_memory_free(req);
	_memory_free(rsp);
	*mqtt_handle = client;
	return 0;

end_error:
	if (req) {
		_memory_free(req);
	}
	if (rsp) {
		_memory_free(rsp);
	}

	if (ret != 0) {
		_tcp_disconnect(sock);
	}

	if (client) {
		_memory_free(client);
	}
	return ret;
}

int mqtt_disconnect(void *mqtt_handle) {
	int ret, msg_len;
	char *msg;
	_MQTT_CLIENT *client = (_MQTT_CLIENT *)mqtt_handle;

	if ((ret = _mqtt_msg_build_disconnect(&msg, &msg_len)) != 0) {
		return ret;
	}

	ret = mqtt_send(client, msg, msg_len, 5000);

	_memory_free(msg);

	_tcp_disconnect(client->sock);

	_memory_free(client);

	return ret;
}

int _mqtt_https_connect(void *sock, _MQTT_COONNECT_PARAMS *params) {
	int ret;
	char http_request[260];
	char reply[256], buf;
	int reply_len = 0, term_char = 0, len = 1;

	snprintf(http_request, 256, "GET /mqtt HTTP/1.1\r\n"
			 "host: %s\r\n"
			 "\r\n", params->host);
	if ((ret = _tcp_send(sock, http_request, strlen(http_request), 1000)) != 0) {
		return ret;
	}

	_memory_memset(reply, 0, 256);

	while (reply_len < 255) {

		if ((ret = _tcp_recv(sock, &buf, &len, 1000)) != 0) {
			return ret;
		}
		reply[reply_len++] = buf;
		if (buf == '\r' || buf == '\n') {
			++term_char;
			if (term_char >= 4) {
				break;
			}
		} else {
			term_char = 0;
		}
	}

	if (strncmp(reply, "HTTP/1.1 200 OK", 15) != 0 || strncmp(reply + strlen(reply) - 4, "\r\n\r\n", 4) != 0) {
		return ERR_TR50_HTTP_PROXY_FAILED;
	}
	return 0;
}

int mqtt_publish(void *mqtt_handle, int qos, int retain, unsigned short message_id, const char *topic, const char *data, int data_len) {
	_MQTT_CLIENT *client = (_MQTT_CLIENT *)mqtt_handle;
	int req_len;
	char *req = NULL;
	int ret;

	if ((ret = _mqtt_msg_build_publish(topic, qos, retain, message_id, data, data_len, &req, &req_len)) != 0) {
		return ret;
	}

	log_hexdump(LOG_TYPE_LOW_LEVEL, "mqtt_publish(): sending", req, req_len);
	if ((ret = mqtt_send(client, req, req_len, 5000)) != 0) {
		log_debug("mqtt_publish(): failed [%d]", ret);
		goto end_error;
	}

end_error:
	if (req) {
		_memory_free(req);
	}
	return ret;
}

int mqtt_puback(void *mqtt_handle, unsigned short message_id, int qos) {
	_MQTT_CLIENT *client = (_MQTT_CLIENT *)mqtt_handle;
	int req_len;
	char *req = NULL;
	int ret;

	if ((ret = _mqtt_msg_build_puback(qos, message_id, &req, &req_len)) != 0) {
		return ret;
	}

	log_hexdump(LOG_TYPE_LOW_LEVEL, "mqtt_puback(): sending", req, req_len);
	if ((ret = mqtt_send(client, req, req_len, 5000)) != 0) {
		log_debug("mqtt_publish(): failed [%d]", ret);
		goto end_error;
	}

end_error:
	if (req) {
		_memory_free(req);
	}
	return ret;
}

int mqtt_ping(void *mqtt_handle) {
	_MQTT_CLIENT *client = (_MQTT_CLIENT *)mqtt_handle;
	int ret, req_len;
	char *req = NULL;

	if ((ret = _mqtt_msg_build_ping(&req, &req_len)) != 0) {
		return ret;
	}

	if ((ret = mqtt_send(client, req, req_len, 5000)) != 0) {
		goto end_error;
	}

end_error:
	if (req) {
		_memory_free(req);
	}
	return ret;
}

int mqtt_subscribe(void *mqtt_handle, unsigned short msg_id, const char *topic, int qos) {
	_MQTT_CLIENT *client = (_MQTT_CLIENT*)mqtt_handle;
	int req_len;
	char *req = NULL;
	int ret;

	if ((ret = _mqtt_msg_build_subscribe(topic, qos, msg_id, &req, &req_len)) != 0) return ret;

	log_hexdump(LOG_TYPE_LOW_LEVEL, "mqtt_subscribe(): sending", req, req_len);
	if ((ret = mqtt_send(client, req, req_len, 5000)) != 0) {
		log_debug("mqtt_subscribe(): failed [%d]", ret);
		goto end_error;
	}

end_error:
	if (req) _memory_free(req);
	return ret;
}

int mqtt_unsubscribe(void *mqtt_handle, unsigned short msg_id, const char *topic) {
	_MQTT_CLIENT *client = (_MQTT_CLIENT*)mqtt_handle;
	int req_len;
	char *req = NULL;
	int ret;

	if ((ret = _mqtt_msg_build_unsubscribe(topic, msg_id, &req, &req_len)) != 0) return ret;

	log_hexdump(LOG_TYPE_LOW_LEVEL, "mqtt_unsubscribe(): sending", req, req_len);
	if ((ret = mqtt_send(client, req, req_len, 5000)) != 0) {
		log_debug("mqtt_unsubscribe(): failed [%d]", ret);
		goto end_error;
	}

end_error:
	if (req) _memory_free(req);
	return ret;
}

long long mqtt_stats_byte_sent(void *mqtt_handle) {
	_MQTT_CLIENT *client = (_MQTT_CLIENT *)mqtt_handle;
	if (client == NULL) {
		return 0;
	}
	return client->byte_sent;
}

long long mqtt_stats_byte_recv(void *mqtt_handle) {
	_MQTT_CLIENT *client = (_MQTT_CLIENT *)mqtt_handle;
	if (client == NULL) {
		return 0;
	}
	return client->byte_recv;
}

void mqtt_stats_clear(void *mqtt_handle) {
	_MQTT_CLIENT *client = (_MQTT_CLIENT *)mqtt_handle;
	if (client == NULL) {
		return;
	}
	client->byte_recv = 0;
	client->byte_sent = 0;
}
