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

#ifndef _TR50_H_
#define _TR50_H_

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

#include <stddef.h>
#include <tr50/util/json.h>
#include <tr50/error.h>

#if !defined(TRUE)
#  define TRUE 1
#endif
#if !defined(FALSE)
#  define FALSE 0
#endif
#if !defined(NULL)
#  define NULL 0
#endif

//global setting
TR50_EXPORT int			tr50_global_ssl_config_set(const char *password, const char *file, int verify_peer);
TR50_EXPORT void		tr50_log_set_low_level();
TR50_EXPORT void		tr50_log_set_debug();

// Message building
TR50_EXPORT int			tr50_message_create(void **message);
TR50_EXPORT int			tr50_message_add_command(void *message, const char *cmd_id, const char *cmd, JSON *params);
TR50_EXPORT int			tr50_message_delete(void *message);

TR50_EXPORT int         tr50_reply_get_error_code(void *reply, const char *cmd_id);
TR50_EXPORT const char *tr50_reply_get_error_message(void *reply, const char *cmd_id);
TR50_EXPORT int         tr50_reply_get_error(void *reply, const char *cmd_id, char **error_message);
TR50_EXPORT JSON       *tr50_reply_get_params(void *reply, const char *cmd_id, int detach);
TR50_EXPORT int			tr50_reply_delete(void *reply);

TR50_EXPORT void		tr50_message_printf(const char *who, void*message);
TR50_EXPORT int			tr50_message_from_string(const char *payload, int payload_len, void **tr50_message);
TR50_EXPORT int			tr50_message_to_string(const char *topic, void *message, char **data, int *data_len);

// connect/disconnect
TR50_EXPORT int			tr50_create(void **tr50, const char *client_id, const char *host, int port);
TR50_EXPORT int			tr50_start(void *tr50);
TR50_EXPORT int			tr50_start2(void *tr50, int *connect_error);
TR50_EXPORT int			tr50_stop(void *tr50);
TR50_EXPORT int			tr50_delete(void *tr50);

#define TR50_STATE_STOPPED			0
#define TR50_STATE_STARTED			1

#define TR50_STATUS_CREATED			0
#define TR50_STATUS_CONNECTING		1
#define TR50_STATUS_CONNECTED		2
#define TR50_STATUS_BROKEN			3
#define TR50_STATUS_RECONNECTING	4
#define TR50_STATUS_DISABLED		5
#define TR50_STATUS_STOPPING		6
#define TR50_STATUS_STOPPED			7

TR50_EXPORT const char *tr50_status_to_string(int status);
TR50_EXPORT int tr50_current_state(void *tr50);
TR50_EXPORT int tr50_current_status(void *tr50);
TR50_EXPORT const char *tr50_current_state_string(void *tr50);
TR50_EXPORT const char *tr50_current_status_string(void *tr50);

// main functions
typedef void (*tr50_async_reply_callback)(void * tr50, int status, const void *request_message, void *reply_message, void *custom);
TR50_EXPORT int tr50_api_call(void *tr50,void *message);
TR50_EXPORT int tr50_api_call_async(void *tr50, void *message, int *seq_id, tr50_async_reply_callback callback, void *custom, int timeout);
TR50_EXPORT int tr50_api_call_sync(void *tr50, void *message, void **reply, int timeout);
typedef void (*tr50_async_raw_reply_callback)(int status, const char *reply_json, void *custom);
TR50_EXPORT int tr50_api_raw_async(void *tr50, const char *request_json, int *seq_id, tr50_async_raw_reply_callback callback, void *custom, int timeout);
TR50_EXPORT int tr50_api_raw_sync(void *tr50, const char *request_json, char **reply_json, int timeout);
// method
typedef int(*tr50_method_callback)(void * tr50, const char *id, const char *thing_key, const char *method, const char *from, JSON *params, void * custom);
TR50_EXPORT int tr50_method_register(void *tr50, const char *method, tr50_method_callback callback);
TR50_EXPORT int tr50_method_process(void *tr50, const char *id, const char *thing_key, const char *method, const char *from, JSON *params, void *custom);
TR50_EXPORT int tr50_method_update(void *tr50, const char *id, const char *msg);
TR50_EXPORT int tr50_method_ack(void *tr50, const char *id, int status, const char *err_message, JSON *ack_params);
TR50_EXPORT int tr50_is_method_registered();
// Command
typedef int (*tr50_command_callback)(const char *id, const char *thing_key, const char *command, const char *from, JSON *params);
TR50_EXPORT int tr50_command_register(void *tr50, const char *command, tr50_command_callback callback);
TR50_EXPORT int tr50_command_process(void *tr50, const char *id, const char *thing_key, const char *command, const char *from, JSON *params, void *custom);
TR50_EXPORT int tr50_command_update(void *tr50, const char *id, const char *msg);
TR50_EXPORT int tr50_command_ack(void *tr50, const char *id, int status, const char *err_message, JSON *ack_params);

// Statistic
TR50_EXPORT int tr50_pending_count(void *tr50);
TR50_EXPORT int tr50_pending_expired_count(void *tr50);

// configuration
typedef void (*tr50_async_non_api_callback)(const char *topic, const char *data, int data_len, void *custom);
TR50_EXPORT int			tr50_config_set_non_api_handler(void *tr50, tr50_async_non_api_callback callback, void *custom);
typedef void (*tr50_async_state_change_callback)(int previous_state, int current_state, const char *why, void *custom);
TR50_EXPORT int			tr50_config_set_state_change_handler(void *tr50, tr50_async_state_change_callback callback, void *custom);
typedef void(*tr50_async_api_watcher_callback)(const char *data, int len, int is_reply);
TR50_EXPORT int			tr50_config_set_api_watcher_handler(void *tr50, tr50_async_api_watcher_callback callback);
typedef int(*tr50_async_should_reconnect_callback)(int disconnected_in_ms, int last_reconnect_in_ms, void *custom);
TR50_EXPORT int			tr50_config_set_should_reconnect_handler(void *tr50, tr50_async_should_reconnect_callback callback, void *custom);
TR50_EXPORT int			tr50_config_set_host(void *tr50, const char *host);
TR50_EXPORT int			tr50_config_set_port(void *tr50, int port);
TR50_EXPORT int			tr50_config_set_keeplive(void *tr50, int keepalive_in_ms);
TR50_EXPORT int			tr50_config_set_client_id(void *tr50, const char *client_id);
TR50_EXPORT int			tr50_config_set_timeout(void *tr50, int timeout_in_ms);
TR50_EXPORT int			tr50_config_set_username(void *tr50, const char *username);
TR50_EXPORT int			tr50_config_set_password(void *tr50, const char *password);
TR50_EXPORT int			tr50_config_set_ssl(void *tr50, int enabled);
TR50_EXPORT int			tr50_config_set_https_proxy(void *tr50, int enabled);
TR50_EXPORT int			tr50_config_set_compress(void *tr50, int enabled);
#define TR50_PROXY_TYPE_NONE	0
#define TR50_PROXY_TYPE_HTTP	1
#define TR50_PROXY_TYPE_SOCK4	2
#define TR50_PROXY_TYPE_SOCK4A	3
#define TR50_PROXY_TYPE_SOCK5	4
TR50_EXPORT int			tr50_config_set_proxy(void *tr50, int type, const char *addr, const char *username, const char *password);

TR50_EXPORT const char *tr50_config_get_host(void *tr50);
TR50_EXPORT int			tr50_config_get_port(void *tr50);
TR50_EXPORT int			tr50_config_get_keeplive(void *tr50);
TR50_EXPORT const char *tr50_config_get_client_id(void *tr50);
TR50_EXPORT int			tr50_config_get_timeout(void *tr50);
TR50_EXPORT const char *tr50_config_get_username(void *tr50);
TR50_EXPORT const char *tr50_config_get_password(void *tr50);

TR50_EXPORT int			tr50_stats_byte_recv(void *tr50);
TR50_EXPORT int			tr50_stats_byte_sent(void *tr50);
TR50_EXPORT int			tr50_stats_pub_recv(void *tr50);
TR50_EXPORT int			tr50_stats_pub_sent(void *tr50);
TR50_EXPORT int			tr50_stats_compress_ratio(void *tr50);
TR50_EXPORT long long	tr50_stats_last_connected(void *tr50);
TR50_EXPORT int			tr50_stats_reconnect_attempt_count(void *tr50);
TR50_EXPORT int			tr50_stats_reconnect_count(void *tr50);
TR50_EXPORT int			tr50_stats_last_error(void *tr50);
TR50_EXPORT int			tr50_stats_notify(void *tr50);
TR50_EXPORT int			tr50_stats_mailbox_check(void *tr50);
TR50_EXPORT int			tr50_stats_in_api_call_async(void *tr50);
TR50_EXPORT int			tr50_stats_in_api_raw_async(void *tr50);

TR50_EXPORT void		tr50_stats_clear_compression_ratio(void *tr50);
TR50_EXPORT void		tr50_stats_clear_send_recv(void *tr50);
TR50_EXPORT void		tr50_stats_clear_last_error(void *tr50);

// Misc
TR50_EXPORT int			tr50_mailbox_suspend(void *tr50);
TR50_EXPORT int			tr50_mailbox_resume(void *tr50, int check_immediately);
TR50_EXPORT int			tr50_mailbox_check(void *tr50);
TR50_EXPORT int			tr50_api_msg_id_next(void *tr50);
typedef void (*tr50_mqtt_qos_callback)(int status, void *custom);
TR50_EXPORT int			tr50_mqtt_subscribe(void *tr50, const char *topic_filter, int qos, tr50_mqtt_qos_callback qos_callback, void *qos_callback_custom);
TR50_EXPORT int			tr50_mqtt_unsubscribe(void *tr50, const char *topic_filter, tr50_mqtt_qos_callback qos_callback, void *qos_callback_custom);
TR50_EXPORT int			tr50_mqtt_publish(void *tr50, const char *topic, const char *payload, int length, int retain);
TR50_EXPORT int			tr50_mqtt_publish_qos1(void *tr50, const char *topic, const char *payload, int length, int retain, tr50_mqtt_qos_callback qos_callback, void *qos_callback_custom);

#ifdef __cplusplus
}
#endif

#endif // !_TR50_H_
