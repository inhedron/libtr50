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
#include <stdlib.h>
#include <time.h>

#include <tr50/tr50.h>

#include <tr50/internal/tr50.h>

#include <tr50/mqtt/mqtt.h>

#include <tr50/util/compress.h>
#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/mutex.h>
#include <tr50/util/tcp.h>
#include <tr50/util/time.h>

void _tr50_pending_expiration_handler(_TR50_CLIENT *client);
void _tr50_publish_handler(const char *topic, const char *data, int data_len, void *custom);
void _tr50_api_watcher_reply(_TR50_CLIENT *client, const char *data, int data_len);
static void _tr50_state_change_handler(int previous_mqtt_state, int current_mqtt_state, int error, const char *why, void *custom);

int _tr50_should_reconnect_callback(int disconnected_in_ms, int last_reconnect_in_ms, void *custom) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)custom;
	if (client->should_reconnect_callback) {
		return client->should_reconnect_callback(disconnected_in_ms, last_reconnect_in_ms, client->should_reconnect_custom);
	}
	return last_reconnect_in_ms >= 60000;
}

int tr50_create(void **tr50, const char *client_id, const char *host, int port) {
	_TR50_CLIENT *client;
	if ((client = (_TR50_CLIENT *)_memory_malloc(sizeof(_TR50_CLIENT))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(client, 0, sizeof(_TR50_CLIENT));

	// populate config
	tr50_config_set_client_id(client, client_id);
	tr50_config_set_host(client, host);
	tr50_config_set_port(client, port);

	tr50_config_set_timeout(client, 5000);
	tr50_config_set_keeplive(client, 60000);

	// creating objects
	tr50_pending_create(client);
	_tr50_mutex_create(&client->mux);
	_tr50_mutex_create(&client->stats.mux);
	_tr50_mutex_create(&client->mailbox_check_mux);
	
	*tr50 = client;
	return 0;
}

int tr50_delete(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;

	_tr50_mutex_delete(client->mailbox_check_mux);
	_tr50_mutex_delete(client->mux);
	_tr50_mutex_delete(client->stats.mux);
	tr50_pending_delete(client);
	_tr50_config_delete(&client->config);
	return 0;
}

int tr50_global_ssl_config_set(const char *password, const char *file, int verify_peer) {
	return _tcp_ssl_config(password, file, verify_peer);
}

int tr50_start2(void *tr50, int *connect_error) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	_TR50_CONFIG *config = &client->config;
	int ret = 0;

	_tr50_mutex_lock(client->mux);

	if (client->state == TR50_STATE_STARTED) {
		ret = ERR_TR50_ALREADY_STARTED;
		goto end_error;
	}

	// copy config into current config
	mqtt_connect_params_create(&client->connect_params, config->client_id, config->host, config->port, config->keepalive_in_ms / 1000);

	if (config->proxy_type > 0) {
		mqtt_connect_params_use_proxy(client->connect_params, config->proxy_type, config->proxy_addr, config->proxy_username, config->proxy_password);
	}

	if (config->ssl) {
		mqtt_connect_params_use_ssl(client->connect_params);
	}
	if (config->https_proxy) {
		mqtt_connect_params_use_https_proxy(client->connect_params);
	}

	if (config->username && config->password) {
		mqtt_connect_params_set_username(client->connect_params, config->username, config->password);
	}

	client->compress = config->compress;
	tr50_stats_clear_compression_ratio(client);

	client->should_reconnect_callback = config->should_reconnect_callback;
	client->should_reconnect_custom = config->should_reconnect_custom;

	client->non_api_callback = config->non_api_handler;
	client->non_api_callback_custom = config->non_api_handler_custom;

	client->state = TR50_STATE_STARTED;

	// connect!
	if ((ret = mqtt_async_connect(&client->mqtt, client->connect_params, _tr50_publish_handler, _tr50_state_change_handler, _tr50_should_reconnect_callback, client, connect_error)) != 0) {
		client->state = TR50_STATE_STOPPED;
		goto end_error;
	}

	_tr50_mutex_unlock(client->mux);
	return 0;

end_error:
	_tr50_mutex_unlock(client->mux);
	return ret;
}

int tr50_start(void *tr50) {
	return tr50_start2(tr50, NULL);
}

int tr50_stop(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	int ret, last_sent, last_recv;

	_tr50_mutex_lock(client->mux);
	if (client->state == TR50_STATE_STOPPED) {
		ret = ERR_TR50_ALREADY_STOPPED;
		goto end_error;
	}
	if (client->is_stopping) {
		ret = ERR_TR50_STOPPING;
		goto end_error;
	}

	client->is_stopping = 1;
	last_sent = mqtt_async_stats_byte_sent(client->mqtt);
	last_recv = mqtt_async_stats_byte_recv(client->mqtt);
	_tr50_mutex_unlock(client->mux);

	// release the mux because one of the publish callback might try to grab it.
	if ((ret = mqtt_async_disconnect(client->mqtt)) != 0) {
		_tr50_mutex_lock(client->mux);
		client->is_stopping = 0;
		goto end_error;
	}

	_tr50_mutex_lock(client->mux);
	if (client->connect_params) {
		mqtt_connect_params_delete(client->connect_params);
		client->connect_params = NULL;
	}
	client->mqtt = NULL;
	client->state = TR50_STATE_STOPPED;
	client->is_stopping = 0;
	client->stats.total_sent += last_sent;
	client->stats.total_recv += last_recv;
	_tr50_mutex_unlock(client->mux);
	return 0;

end_error:
	log_important_info("tr50_stop(): failed [%d]", ret);
	_tr50_mutex_unlock(client->mux);
	return ret;
}

const char *tr50_status_to_string(int status) {
	switch (status) {
	case TR50_STATUS_CREATED:
		return "Stopped";
	case TR50_STATUS_CONNECTING:
		return "Connecting";
	case TR50_STATUS_CONNECTED:
		return "Connected";
	case TR50_STATUS_BROKEN:
		return "Broken";
	case TR50_STATUS_RECONNECTING:
		return "Reconnecting";
	case TR50_STATUS_DISABLED:
		return "Disabled";
	case TR50_STATUS_STOPPING:
		return "Stopping";
	case TR50_STATUS_STOPPED:
		return "Stopped";
	}
	return "Unknown";
}

int tr50_current_state(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	return client->state;
}

int tr50_current_status(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	int mqtt_state;

	_tr50_mutex_lock(client->mux);
	if (client->is_stopping) {
		_tr50_mutex_unlock(client->mux);
		return TR50_STATUS_STOPPING;
	}
	if (client->state == TR50_STATE_STOPPED) {
		_tr50_mutex_unlock(client->mux);
		return TR50_STATUS_STOPPED;
	}

	mqtt_state = mqtt_async_state(client->mqtt);
	_tr50_mutex_unlock(client->mux);
	return mqtt_state;
}

const char *tr50_current_state_string(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	switch (client->state) {
	case TR50_STATE_STARTED:
		return "Started";
	case TR50_STATE_STOPPED:
		return "Stopped";
	}
	return "Unknown";
}

const char *tr50_current_status_string(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	int mqtt_state;

	_tr50_mutex_lock(client->mux);
	if (client->is_stopping) {
		_tr50_mutex_unlock(client->mux);
		return "Disconnecting";
	}
	if (client->state == TR50_STATE_STOPPED) {
		_tr50_mutex_unlock(client->mux);
		return "Stopped";
	}

	mqtt_state = mqtt_async_state(client->mqtt);
	_tr50_mutex_unlock(client->mux);

	switch (mqtt_state) {
	case MQTT_ASYNC_CLIENT_STATE_CONNECTING:
		return "Connecting";
	case MQTT_ASYNC_CLIENT_STATE_CONNECTED:
		return "Connected";
	case MQTT_ASYNC_CLIENT_STATE_BROKEN:
		return "Broken";
	case MQTT_ASYNC_CLIENT_STATE_RECONNECTING:
		return "Reconnecting";
	case MQTT_ASYNC_CLIENT_STATE_DISABLED:
		return "Disabled";
	case MQTT_ASYNC_CLIENT_STATE_DELETING:
		return "Disconnecting";
	case MQTT_ASYNC_CLIENT_STATE_DELETED:
		return "Stopped";
	}
	return "Unknown";
}

void _tr50_publish_handle_publish(_TR50_CLIENT *client, const char *data, int data_len, int seq_id) {
	_TR50_MESSAGE *request, *reply;
	int ret;

	if ((request = (_TR50_MESSAGE *)tr50_pending_find_and_remove(client, seq_id)) == NULL) {
		log_important_info("tr50_pending_find_and_remove(): message[%d] not in pending", seq_id);
		return;
	}

	if (request->message_type == TR50_MESSAGE_TYPE_RAW) {
		if (request->raw_callback) {
			((tr50_async_raw_reply_callback)request->raw_callback)(0, data, request->callback_custom);
		}
	} else if (request->message_type == TR50_MESSAGE_TYPE_OBJ) {
		if ((ret = tr50_message_from_string(data, data_len, (void *)&reply)) != 0) {
			_tr50_api_watcher_reply(client, data, data_len);
			log_important_info("_tr50_publish_handler(): Invalid message recv'ed length[%d].", data_len);
			return;
		}
		if (!reply->is_reply) {
			log_important_info("_tr50_publish_handler(): TR50 does not support PUSH request.");
			tr50_message_delete(reply);
			return;
		}
		reply->seq_id = seq_id;
		if (request->reply_callback) {
			long long ended, started = _time_now();
			((tr50_async_reply_callback)request->reply_callback)(client, 0, request, reply, request->callback_custom);
			ended = _time_now();
			if (ended - started > 1000) {
				log_need_investigation("_tr50_publish_handle_publish(): reply callback for [%d] is taking [%d]ms.", request->seq_id, (int)(ended - started));
			}
		} else {
			tr50_message_delete(reply);
		}
	}

	_tr50_stats_pub_recv_up(client, data_len); // compress or not?
	_tr50_api_watcher_reply(client, data, data_len);
	tr50_message_delete(request);
}

void _tr50_api_watcher_reply(_TR50_CLIENT *client, const char *data, int data_len) {
	if (client->config.api_watcher_handler) {
		client->config.api_watcher_handler(data, data_len, 1);
	}
}

void _tr50_publish_handler(const char *topic, const char *data, int data_len, void *custom) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)custom;
	int ret;
	
	if (client->non_api_callback) {
		client->non_api_callback(topic, data, data_len, client->non_api_callback_custom);
	}

	if (strncmp(topic, "replyz/", 7) == 0) {
		char *out;
		int out_len = 0;

		if ((ret = _compress_inflate(data, data_len, &out, &out_len)) != 0) {
			log_important_info("_tr50_publish_handler(): Decompression failed [%d].", ret);
			return;
		}
		_tr50_stats_set_compress_ratio(client, out_len, data_len);

		_tr50_publish_handle_publish(client, out, out_len, atoi(topic[0] == 'a' ? topic + 5 : topic + 7));
		_memory_free(out);
	} else if (strncmp(topic, "reply/", 6) == 0) {
		_tr50_publish_handle_publish(client, data, data_len, atoi(topic[0] == 'a' ? topic + 4 : topic + 6));
	} else { // Non-tr50 requests
		if (strcmp(topic, "notify/mailbox_activity") == 0) {
			_tr50_stats_notify_up(client);
			tr50_mailbox_check(client);
		}
	}
}

static void _tr50_state_change_handler(int previous_mqtt_state, int current_mqtt_state, int error, const char *why, void *custom) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)custom;

	_tr50_stats_set_last_error(client, error, why);
	if (current_mqtt_state == MQTT_ASYNC_CLIENT_STATE_CONNECTED) {
		_tr50_stats_set_connected(client);
	}
	if (client->config.state_change_handler) {
		client->config.state_change_handler(previous_mqtt_state, current_mqtt_state, why, client->config.state_change_handler_custom);
	}
}


void tr50_log_set_low_level() {
	log_filter_maximum_log_level(LOG_TYPE_LOW_LEVEL);
}

void tr50_log_set_debug() {
	log_filter_maximum_log_level(LOG_TYPE_DEBUG);
}

int tr50_mqtt_subscribe(void *tr50, const char *topic_filter, int qos, tr50_mqtt_qos_callback callback, void *custom) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	int ret = 0;

	_tr50_mutex_lock(client->mux);
	if (client->state == TR50_STATE_STOPPED) {
		ret = ERR_TR50_NOT_CONNECTED;
		goto end_error;
	}
	if (client->is_stopping) {
		ret = ERR_TR50_STOPPING;
		goto end_error;
	}

	ret = mqtt_async_subscribe(client->mqtt, topic_filter, qos, callback, custom);
end_error:
	_tr50_mutex_unlock(client->mux);
	return ret;
}

int tr50_mqtt_unsubscribe(void *tr50, const char *topic_filter, tr50_mqtt_qos_callback callback, void *custom) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	int ret = 0;

	_tr50_mutex_lock(client->mux);
	if (client->state == TR50_STATE_STOPPED) {
		ret = ERR_TR50_NOT_CONNECTED;
		goto end_error;
	}
	if (client->is_stopping) {
		ret = ERR_TR50_STOPPING;
		goto end_error;
	}

	ret = mqtt_async_unsubscribe(client->mqtt, topic_filter, callback, custom);
end_error:
	_tr50_mutex_unlock(client->mux);
	return ret;
}

int tr50_mqtt_publish_base(void *tr50, const char *topic, const char *payload, int length, int retain, int qos, tr50_mqtt_qos_callback callback, void *custom) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	int ret = 0;

	_tr50_mutex_lock(client->mux);
	if (client->state == TR50_STATE_STOPPED) {
		ret = ERR_TR50_NOT_CONNECTED;
		goto end_error;
	}
	if (client->is_stopping) {
		ret = ERR_TR50_STOPPING;
		goto end_error;
	}
	if (qos == 0) {
		ret = mqtt_async_publish(client->mqtt, topic, payload, length, retain);
	} else {
		ret = mqtt_async_publish_qos1(client->mqtt, topic, payload, length, retain, callback, custom);
	}
end_error:
	_tr50_mutex_unlock(client->mux);

	return ret;
}

int	tr50_mqtt_publish(void *tr50, const char *topic, const char *payload, int length, int retain) {
	return tr50_mqtt_publish_base(tr50, topic, payload, length, retain, 0, NULL, NULL);
}

int tr50_mqtt_publish_qos1(void *tr50, const char *topic, const char *payload, int length, int retain, tr50_mqtt_qos_callback callback, void *custom) {
	return tr50_mqtt_publish_base(tr50, topic, payload, length, retain, 1, callback, custom);
}
