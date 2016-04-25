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

#include <tr50/tr50.h>

#define TR50_COMMAND_NAME_LEN	64
#define TR50_METHOD_NAME_LEN	64

typedef struct {
	int						hash;
	char					name[TR50_COMMAND_NAME_LEN];
	tr50_command_callback	callback;
	void *					next;
} _TR50_COMMAND; 

typedef struct {
	int						hash;
	char					name[TR50_METHOD_NAME_LEN];
	tr50_method_callback	callback;
	void *					next;
} _TR50_METHOD;

typedef struct {
	char *	client_id;
	char *	host;
	int 	port;
	int		ssl;
	int     https_proxy;

	int		keepalive_in_ms;
	int		timeout_in_ms;
	int		compress;

	char *	username;
	char *	password;

	int		proxy_type;
	char *	proxy_addr;
	char *	proxy_username;
	char *	proxy_password;

	tr50_async_should_reconnect_callback should_reconnect_callback;
	void * should_reconnect_custom;
	tr50_async_non_api_callback non_api_handler;
	void *	non_api_handler_custom;
	tr50_async_state_change_callback state_change_handler;
	void *	state_change_handler_custom;
	tr50_async_api_watcher_callback api_watcher_handler;
} _TR50_CONFIG;

#define _TR50_COMPRESSION_HISTORY_MAX		100
#define _TR50_STATS_LAST_ERROR_MESSAGE_LEN	128
typedef struct {
	void *		mux;
	int			pub_sent;
	int			pub_recv;
	int			pub_byte_sent;
	int			pub_byte_recv;
	long long	total_sent;
	long long	total_recv;
	long long	connected_timestamp;
	int			connected_count;
	int			notify_count;
	int			mailbox_check_count;
	
	int			last_error;
	long long	last_error_timestamp;
	char		last_error_message[_TR50_STATS_LAST_ERROR_MESSAGE_LEN + 1];

	int         compression_compressed_len[_TR50_COMPRESSION_HISTORY_MAX];
	int			compression_original_len[_TR50_COMPRESSION_HISTORY_MAX];
	int			compression_ratio_count;

	int			in_api_call_async;
	int			in_api_raw_async;
} _TR50_STATS;

typedef struct {
	void *	list_head;
	void *	list_tail;
	void *	mux;
	int		count;
	void *	thread;
	int		is_deleting;
	int		expired_count;
} _TR50_PENDING;

typedef struct {
// internal
	void *	connect_params;
	void *	mqtt;
	void *	mux;
	int		state;
	int		seq_id;
	int		compress;
	int		is_stopping;

	tr50_async_should_reconnect_callback should_reconnect_callback;
	void * should_reconnect_custom;

	tr50_async_non_api_callback non_api_callback;
	void *	non_api_callback_custom;

// command
	_TR50_COMMAND *command_head;

// pending
	_TR50_PENDING pending;

// config
	_TR50_CONFIG config;

// stats
	_TR50_STATS stats;

// method
	_TR50_METHOD *method_head;

// mailbox
	int	mailbox_suspend;
	void * mailbox_check_mux;
	int mailbox_send_in_progress;
	int mailbox_another_request;

} _TR50_CLIENT;

#define TR50_MESSAGE_TYPE_OBJ	1
#define TR50_MESSAGE_TYPE_RAW	2

typedef struct {
	int		message_type;
	int		compress_ratio;
	int		seq_id;
	long long pending_sent_timestamp;
	long long pending_expiration_timestamp;
	void *	pending_previous;
	void *	pending_next;

	JSON *  json;
	int		is_reply;

	int		callback_timeout;
	void *	callback_custom;

	void *	raw_callback;
	void *	reply_callback;
} _TR50_MESSAGE;

#define TR50_TOPIC_API				"api"
#define TR50_TOPIC_REPLY			"reply"
#define TR50_TOPIC_COMPRESS_API		"apiz"
#define TR50_TOPIC_COMPRESS_REPLY	"replyz"

// Pending
int tr50_pending_create(_TR50_CLIENT *client);
int tr50_pending_delete(_TR50_CLIENT *client);
int tr50_pending_add(_TR50_CLIENT *client, _TR50_MESSAGE *message);
_TR50_MESSAGE *tr50_pending_find_and_remove(_TR50_CLIENT *client, int hash);

// Stats
void _tr50_stats_pub_recv_up(_TR50_CLIENT *client, int byte_recv);
void _tr50_stats_pub_sent_up(_TR50_CLIENT *client, int byte_sent);
void _tr50_stats_set_compress_ratio(_TR50_CLIENT *client, int data_len, int compressed_len);
void _tr50_stats_set_connected(_TR50_CLIENT *client);
void _tr50_stats_latency(_TR50_CLIENT *client, int latency);
void _tr50_stats_set_last_error(_TR50_CLIENT *client, int error, const char *error_message);
void _tr50_stats_notify_up(_TR50_CLIENT *client);
void _tr50_stats_mailbox_check_up(_TR50_CLIENT *client);

// Config
void _tr50_config_delete(_TR50_CONFIG *config);
