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
#ifndef _TR50_MQTT_H_
#define _TR50_MQTT_H_

#include <tr50/error.h>

typedef void(*mqtt_async_publish_callback)(const char *topic, const char *data, int len, void *custom);
typedef void(*mqtt_async_state_change_callback)(int previous_state, int current_state, int status, const char *why, void *custom);
typedef int(*mqtt_async_should_reconnect_callback)(int disconnected_in_ms, int last_reconnect_in_ms, void *custom);
typedef void(*mqtt_qos_callback)(int status, void *custom);

#if defined(_WIN32)
#  if defined(EXPORT_TR50_SYMS)
#    define TR50_EXPORT __declspec(dllexport)
#  else
#    define TR50_EXPORT __declspec(dllimport)
#  endif
#else
#  define TR50_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

TR50_EXPORT int mqtt_connect_params_create(void **connect_params, const char *client_id, const char *host, long port, unsigned short keepalive_in_sec);
TR50_EXPORT int mqtt_connect_params_set_username(void *connect_params, const char *username, const char *password);
TR50_EXPORT int mqtt_connect_params_delete(void *connect_params);
TR50_EXPORT unsigned short mqtt_connect_params_get_keepalive(void *connect_params);
TR50_EXPORT int mqtt_connect_params_use_ssl(void *connect_params);
TR50_EXPORT int mqtt_connect_params_use_https_proxy(void *connect_params);
TR50_EXPORT int mqtt_connect_params_use_proxy(void *connect_params, int type, const char *addr, const char *username, const char *password);

TR50_EXPORT int mqtt_async_connect(	void **async_client,
									void *connect_params,
									mqtt_async_publish_callback publish_callback,
									mqtt_async_state_change_callback state_change_callback,
									mqtt_async_should_reconnect_callback should_reconnect_callback,
									void *custom,
									int *connect_error);
TR50_EXPORT int	mqtt_async_publish(void *async_client, const char *topic, const char *data, int len, int retain);
TR50_EXPORT int	mqtt_async_publish_qos1(void *async_client, const char *topic, const char *data, int len, int retain, mqtt_qos_callback callback, void *callback_custom);
TR50_EXPORT int	mqtt_async_subscribe(void *async_client, const char *topic, int qos, mqtt_qos_callback callback, void *callback_custom);
TR50_EXPORT int	mqtt_async_unsubscribe(void *async_client, const char *topic, mqtt_qos_callback callback, void *callback_custom);
TR50_EXPORT int mqtt_async_disconnect(void *async_client);

#ifdef __cplusplus
}
#endif

#define MQTT_SCADA_BYTE_1 127L
#define MQTT_SCADA_BYTE_2 16383L
#define MQTT_SCADA_BYTE_3 2097151L
#define MQTT_SCADA_BYTE_4 268435455L

/* A macro to calulate the size of the fixed header */
#define MQTT_CALC_FHEADER_LENGTH( DLen, FHLen ) { \
    if ( DLen <= MQTT_SCADA_BYTE_1 ) {            \
        FHLen = 2;                               \
    } else if ( DLen <= MQTT_SCADA_BYTE_2 ) {     \
        FHLen = 3;                               \
    } else if ( DLen <= MQTT_SCADA_BYTE_3 ) {     \
        FHLen = 4;                               \
    } else if ( DLen <= MQTT_SCADA_BYTE_4 ) {     \
        FHLen = 5;                               \
    } else {                                     \
        FHLen = -1;                              \
    }                                            \
}
    

/* Mask to get the message type from a MQIsdp message */
#define MQTT_GET_MSG_TYPE 0xF0

/* define all the SCADA message types */
#define MQTT_MSG_TYPE_UNKNOWN     0x00
#define MQTT_MSG_TYPE_CONNECT     0x10
#define MQTT_MSG_TYPE_CONNACK     0x20
#define MQTT_MSG_TYPE_PUBLISH     0x30
#define MQTT_MSG_TYPE_PUBACK      0x40
#define MQTT_MSG_TYPE_PUBREC      0x50
#define MQTT_MSG_TYPE_PUBREL      0x60
#define MQTT_MSG_TYPE_PUBCOMP     0x70
#define MQTT_MSG_TYPE_SUBSCRIBE   0x80
#define MQTT_MSG_TYPE_SUBACK      0x90
#define MQTT_MSG_TYPE_UNSUBSCRIBE 0xA0
#define MQTT_MSG_TYPE_UNSUBACK    0xB0
#define MQTT_MSG_TYPE_PINGREQ     0xC0
#define MQTT_MSG_TYPE_PINGRESP    0xD0
#define MQTT_MSG_TYPE_DISCONNECT  0xE0

/* Fixed header options */
#define MQTT_OPT_RETAIN			0x01
#define MQTT_OPT_QOS_1			0x02
#define MQTT_OPT_QOS_2			0x04
#define MQTT_OPT_DUPLICATE		0x08
#define MQTT_OPT_QOS_FAILURE	0x80

/* Subscribe payload QoS options */
#define MQTT_PAYLOAD_QOS_0       0x00
#define MQTT_PAYLOAD_QOS_1       0x01
#define MQTT_PAYLOAD_QOS_2       0x02

typedef struct {
	void *		sock;
	long long	last_recv;

	long long	byte_sent;
	long long	byte_recv;
} _MQTT_CLIENT;


int mqtt_connect_params_create(void **connect_params, const char *client_id, const char *host, long port, unsigned short keepalive_in_sec);
int mqtt_connect_params_set_username(void *connect_params, const char *username, const char *password);
int mqtt_connect_params_delete(void *connect_params);
unsigned short mqtt_connect_params_get_keepalive(void *connect_params);
int mqtt_connect_params_use_ssl(void *connect_params);
int mqtt_connect_params_use_https_proxy(void *connect_params);
#define MQTT_PROXY_TYPE_NONE	0
#define MQTT_PROXY_TYPE_HTTP	1
#define MQTT_PROXY_TYPE_SOCK4	2
#define MQTT_PROXY_TYPE_SOCK4A	3
#define MQTT_PROXY_TYPE_SOCK5	4

int mqtt_connect_params_use_proxy(void *connect_params, int type, const char *addr, const char *username, const char *password);

// basic building blocks
int mqtt_connect(void **mqtt_handle, void *connect_params);
int mqtt_disconnect(void *mqtt_handle);
int mqtt_send(void *mqtt_handle, const char *buf, int len, int timeout);
int mqtt_recv(void *mqtt_handle, char **data, int *len, int timeout);
int mqtt_publish(void *mqtt_handle, int qos, int retain, unsigned short message_id, const char *topic, const char *data, int data_len);
int mqtt_subscribe(void *mqtt_handle, unsigned short msg_id, const char *topic, int qos);
int mqtt_unsubscribe(void *mqtt_handle, unsigned short msg_id, const char *topic);
int mqtt_ping(void *mqtt_handle);
int mqtt_puback(void *mqtt_handle, unsigned short msg_id, int qos);
long long mqtt_stats_byte_sent(void *mqtt_handle);
long long mqtt_stats_byte_recv(void *mqtt_handle);
void mqtt_stats_clear(void*mqtt_handle);

// functions to process a message from broker
int mqtt_msg_cmd_get(const char *data);
int mqtt_msg_process_connack(const char *data, int data_len);
int mqtt_msg_process_puback(const char *data, int data_len, unsigned short *msg_id);
int mqtt_msg_process_publish(const char *data, int data_len, int *qos, unsigned short *msg_id, char **topic, char **payload, int *payload_len);
int mqtt_msg_process_suback(const char *data, int data_len, unsigned short *msg_id, int *qos);
int mqtt_msg_process_unsuback(const char *data, int data_len, unsigned short *msg_id);

// internal function to build message
int _mqtt_msg_build_connect(const char *client_id, const char *username, const char *password, unsigned short keepalive, char **data, int *data_len);
int _mqtt_msg_build_publish(const char *topic, int qos, int retain, unsigned short msg_id, const char *payload, int payload_len, char **data, int *data_len);
int _mqtt_msg_build_subscribe(const char *topic, int qos, unsigned short msg_id, char **data, int *data_len);
int _mqtt_msg_build_unsubscribe(const char *topic, unsigned short msg_id, char **data, int *data_len);
int _mqtt_msg_build_disconnect(char **data, int *data_len);
int _mqtt_msg_build_ping(char **data, int *data_len);
int _mqtt_msg_build_puback(unsigned short msg_id, int qos, char **data, int *data_len);

int mqtt_qos_create(void **mqtt_qos_handle, unsigned char size, int timeout_in_sec);
int mqtt_qos_delete(void *qos);
int mqtt_qos_add(void *qos, unsigned short msg_id, mqtt_qos_callback callback, void *custom);
int mqtt_qos_signal(void *qos, unsigned short msg_id);
int mqtt_qos_clear(void *qos, int status);
int mqtt_qos_any_expired(void *qos);

int _mqtt_decode_header_length(char *ptr, long ptr_len, int *encoded_size, int *decoded_len);

// async client
#define MQTT_ASYNC_CLIENT_STATE_CREATED		0
#define MQTT_ASYNC_CLIENT_STATE_CONNECTING	1
#define MQTT_ASYNC_CLIENT_STATE_CONNECTED	2
#define MQTT_ASYNC_CLIENT_STATE_BROKEN		3
#define MQTT_ASYNC_CLIENT_STATE_RECONNECTING	4
#define MQTT_ASYNC_CLIENT_STATE_DISABLED	5
#define MQTT_ASYNC_CLIENT_STATE_DELETING	6
#define MQTT_ASYNC_CLIENT_STATE_DELETED		7
//
//typedef void (*mqtt_async_publish_callback)(const char *topic, const char *data, int len, void *custom);
//typedef void (*mqtt_async_state_change_callback)(int previous_state, int current_state, int status, const char *why, void *custom);
//typedef int(*mqtt_async_should_reconnect_callback)(int disconnected_in_ms, int last_reconnect_in_ms, void *custom);
//int mqtt_async_connect(	void **async_client,
//						void *connect_params,
//						mqtt_async_publish_callback publish_callback,
//						mqtt_async_state_change_callback state_change_callback,
//						mqtt_async_should_reconnect_callback should_reconnect_callback,
//						void *custom,
//						int *connect_error);
//int	mqtt_async_publish(void *async_client, const char *topic, const char *data, int len);
//int mqtt_async_disconnect(void *async_client);
int mqtt_async_state(void *async_client);
//void *mqtt_async_base_handle(void *async_client);
int mqtt_async_reconnect_count(void *async_client);
int mqtt_async_reconnect_attempt_count(void *async_client);
long long mqtt_async_stats_byte_sent(void *async_client);
long long mqtt_async_stats_byte_recv(void *async_client);
void mqtt_async_stats_clear(void *async_client);

#endif  //_TR50_MQTT_H_
