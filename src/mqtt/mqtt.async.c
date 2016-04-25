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
#include <tr50/util/mutex.h>
#include <tr50/util/tcp.h>
#include <tr50/util/thread.h>
#include <tr50/util/time.h>

#define MQTT_QOS_DEFAULT_SIZE		32
#define MQTT_QOS_DEFAULT_TIMEOUT	30

typedef struct {
	void 			*connect_params;
	int				timeout_in_ms;
	int				msg_id_seq;

	void 			*callback_custom;
	mqtt_async_should_reconnect_callback	should_reconnect_callback;
	mqtt_async_publish_callback				publish_callback;
	mqtt_async_state_change_callback		state_change_callback;

	int				state;
	void 			*thread;
	int				thread_id;
	int				check_thread_id;
	void 			*mux;
	void 			*mqtt;
	void			*qos;
	int				outstanding_ping;

	int				stats_reconnect_count;
	int				stats_reconnect_attempt_count;
	long long		stats_total_byte_sent;
	long long		stats_total_byte_recv;
} _MQTT_ASYNC_CLIENT;

void *_mqtt_async_handler(void *arg);
void _mqtt_async_state_change(_MQTT_ASYNC_CLIENT *client, int old_state, int new_state, int status, const char *why);

int mqtt_async_connect(void **async_client,
					   void *connect_params,
					   mqtt_async_publish_callback publish_callback,
					   mqtt_async_state_change_callback state_change_callback,
					   mqtt_async_should_reconnect_callback should_reconnect_callback,
					   void *custom,
					   int *connect_error) {
	int ret;
	_MQTT_ASYNC_CLIENT *client;
	void *mqtt;

	if (connect_error) {
		*connect_error = 0;
	}

	if (!publish_callback || !should_reconnect_callback) {
		return ERR_TR50_PARMS;
	}

	if ((client = (_MQTT_ASYNC_CLIENT *)_memory_malloc(sizeof(_MQTT_ASYNC_CLIENT))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(client, 0, sizeof(_MQTT_ASYNC_CLIENT));

	client->connect_params = connect_params;
	client->callback_custom = custom;
	client->publish_callback = publish_callback;
	client->state_change_callback = state_change_callback;
	client->should_reconnect_callback = should_reconnect_callback;

	client->state = MQTT_ASYNC_CLIENT_STATE_CONNECTED;

	client->timeout_in_ms = 5000;

	if ((ret = _tr50_mutex_create(&client->mux)) != 0) {
		_memory_free(client);
		return ret;
	}

	if ((ret = mqtt_qos_create(&client->qos, MQTT_QOS_DEFAULT_SIZE, MQTT_QOS_DEFAULT_TIMEOUT)) != 0) {
		_tr50_mutex_delete(client->mux);
		_memory_free(client);
		return ret;
	}

	*async_client = client;

	_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_CREATED, MQTT_ASYNC_CLIENT_STATE_CONNECTING, 0, "MQTT Client created.");

	if ((ret = mqtt_connect(&mqtt, connect_params)) != 0) {
		*async_client = client;
		_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_CONNECTING, MQTT_ASYNC_CLIENT_STATE_BROKEN, ret, "MQTT Client broken.");
		if (connect_error) {
			*connect_error = ret;
		}
	} else {
		client->mqtt = mqtt;
		_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_CONNECTING, MQTT_ASYNC_CLIENT_STATE_CONNECTED, 0, "MQTT Client connected.");
	}

	if ((ret = _thread_create(&client->thread, "TR50:Recv", _mqtt_async_handler, client)) != 0) {
		mqtt_async_disconnect(client);
		return ret;
	}

	log_important_info("mqtt_async_connected.");
	return 0;
}

int mqtt_async_disconnect(void *async_client) {
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT *)async_client;

	if (!client) {
		return 0;
	}

	log_debug("mqtt_async_disconnect...");
	if (client->mux) {
		_tr50_mutex_lock(client->mux);

		_mqtt_async_state_change(client, client->state, MQTT_ASYNC_CLIENT_STATE_DELETING, 0, "MQTT Client disconnecting...");

		_tr50_mutex_unlock(client->mux);
		if (client->thread) {
			_thread_join(client->thread);
			_thread_delete(client->thread);
		}
		_tr50_mutex_delete(client->mux);

	}
	if (client->mqtt) {
		mqtt_disconnect(client->mqtt);
	}

	_mqtt_async_state_change(client, client->state, MQTT_ASYNC_CLIENT_STATE_DELETED, 0, "MQTT Client disconnected.");

	mqtt_qos_delete(client->qos);

	_memory_free(client);
	return 0;
}

void _mqtt_async_handler_keepalive(_MQTT_ASYNC_CLIENT *client) {
	int ret;
	_MQTT_CLIENT *mqtt = (_MQTT_CLIENT *)client->mqtt;
	int keepalive_in_ms = mqtt_connect_params_get_keepalive(client->connect_params) * 1000;

	if (client->state != MQTT_ASYNC_CLIENT_STATE_CONNECTED) {
		return;
	}
	if (keepalive_in_ms <= 0) {
		return;
	}

	if (client->outstanding_ping > 1) {  // if both two pings dont come back yet, something is wrong.
		_tr50_mutex_lock(client->mux);

		if (client->state == MQTT_ASYNC_CLIENT_STATE_CONNECTED) {
			_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_CONNECTED, MQTT_ASYNC_CLIENT_STATE_BROKEN, 0, "MQTT Heartbeat no response.");
		}

		_tr50_mutex_unlock(client->mux);
		client->outstanding_ping = 0;
		return;
	}

	if (mqtt->last_recv + keepalive_in_ms < _time_now() + client->timeout_in_ms) {
		log_recurring(LOG_TYPE_IMPORTANT_INFO, __FILE__, __LINE__, 60, 0, "mqtt_ping");
		if ((ret = mqtt_ping(client->mqtt)) != 0) {
			_tr50_mutex_lock(client->mux);

			if (client->state == MQTT_ASYNC_CLIENT_STATE_CONNECTED) {
				_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_CONNECTED, MQTT_ASYNC_CLIENT_STATE_BROKEN, 0, "MQTT Heartbeat failed.");
			}

			_tr50_mutex_unlock(client->mux);
		} else {
			++client->outstanding_ping;
		}
	}
}

void _mqtt_async_handler_reconnect(_MQTT_ASYNC_CLIENT *client) {
	long long total_disconnected_in_ms = 0;

	while (client->state == MQTT_ASYNC_CLIENT_STATE_BROKEN) {
		void *mqtt;
		int ret, counter = 0;

		log_important_info("_mqtt_async_handler_reconnect: reconnecting");
		_tr50_mutex_lock(client->mux);

		if (client->state != MQTT_ASYNC_CLIENT_STATE_BROKEN) {
			_tr50_mutex_unlock(client->mux);
			return;
		}

		if (client->mqtt) {
			client->stats_total_byte_recv += mqtt_stats_byte_recv(client->mqtt);
			client->stats_total_byte_sent += mqtt_stats_byte_sent(client->mqtt);
			mqtt_disconnect(client->mqtt);
			client->mqtt = NULL;
		}

		_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_BROKEN, MQTT_ASYNC_CLIENT_STATE_RECONNECTING, 0, "MQTT Reconnecting...");

		++client->stats_reconnect_attempt_count;
		if ((ret = mqtt_connect(&mqtt, client->connect_params)) == 0) {
			client->mqtt = mqtt;
			++client->stats_reconnect_count;
			_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_RECONNECTING, MQTT_ASYNC_CLIENT_STATE_CONNECTED, 0, "MQTT Reconnected.");
			log_important_info("_mqtt_async_handler_reconnect: Connected!");
			_tr50_mutex_unlock(client->mux);
			return;
		}

		_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_RECONNECTING, MQTT_ASYNC_CLIENT_STATE_BROKEN, ret, "MQTT Client broken.  Reconnect failed.");

		_tr50_mutex_unlock(client->mux);

		while (client->state == MQTT_ASYNC_CLIENT_STATE_BROKEN && !client->should_reconnect_callback(total_disconnected_in_ms, counter, client->callback_custom)) {
			log_debug("_mqtt_async_handler_reconnect: reconnecting NOT!");
			_thread_sleep(client->timeout_in_ms);
			counter += client->timeout_in_ms;
			total_disconnected_in_ms += client->timeout_in_ms;
		}
	}
}

int _mqtt_async_handler_process(_MQTT_ASYNC_CLIENT *client, const char *data, int len) {
	int cmd = mqtt_msg_cmd_get(data);

	switch (cmd) {
	case MQTT_MSG_TYPE_PUBLISH: {
		unsigned short msg_id;
		int qos, payload_len, ret;
		char *topic, *payload;
		log_debug("publish recv'ed: cmd[%d] len[%d]", cmd, len);

		if ((ret = mqtt_msg_process_publish(data, len, &qos, &msg_id, &topic, &payload, &payload_len)) != 0) {
			log_need_investigation("publish process(): mqtt_msg_process_publish failed [%d]", ret);
			return ret;
		}
		if (qos > MQTT_PAYLOAD_QOS_0) {
			_tr50_mutex_lock(client->mux);
			mqtt_puback(client->mqtt, msg_id, qos);
			_tr50_mutex_unlock(client->mux);
		}
		log_hexdump(LOG_TYPE_LOW_LEVEL, "mqtt_msg_process_publish():", payload, payload_len);
		client->publish_callback(topic, payload, payload_len, client->callback_custom);
		log_recurring(LOG_TYPE_IMPORTANT_INFO, __FILE__, __LINE__, 5, 0, "publish callback: OK");
		_memory_free(payload);
		_memory_free(topic);
		break;
	}
	case MQTT_MSG_TYPE_PINGRESP:
		client->outstanding_ping = 0;
		log_recurring(LOG_TYPE_IMPORTANT_INFO, __FILE__, __LINE__, 60, 0, "pingresp recv'ed.");
		break;
	case MQTT_MSG_TYPE_PUBACK: {
		unsigned short msg_id;
		int ret;
		
		if ((ret = mqtt_msg_process_puback(data, len, &msg_id)) != 0) {
			log_need_investigation("publish process(): mqtt_msg_process_puback failed [%d]", ret);
			return ret;
		}
		mqtt_qos_signal(client->qos, msg_id);
		log_recurring(LOG_TYPE_IMPORTANT_INFO, __FILE__, __LINE__, 5, 0, "puback recv'ed");
		break;
	}
	case MQTT_MSG_TYPE_SUBACK: {
		unsigned short msg_id;
		int ret, qos;

		if ((ret = mqtt_msg_process_suback(data, len, &msg_id, &qos)) != 0) {
			log_need_investigation("publish process(): mqtt_msg_process_suback failed [%d]", ret);
			return ret;
		}
		mqtt_qos_signal(client->qos, msg_id);
		log_recurring(LOG_TYPE_IMPORTANT_INFO, __FILE__, __LINE__, 5, 0, "suback recv'ed: qos[%d]", qos);
		break;
	}
	case MQTT_MSG_TYPE_UNSUBACK: {
		unsigned short msg_id;
		int ret;

		if ((ret = mqtt_msg_process_unsuback(data, len, &msg_id)) != 0) {
			log_need_investigation("publish process(): mqtt_msg_process_unsuback failed [%d]", ret);
			return ret;
		}
		mqtt_qos_signal(client->qos, msg_id);
		log_recurring(LOG_TYPE_IMPORTANT_INFO, __FILE__, __LINE__, 5, 0, "unsuback recv'ed");
		break;
	}
	default: {
		log_debug("unhandle recv'ed: cmd[%d] len[%d]", cmd, len);
		log_hexdump(LOG_TYPE_LOW_LEVEL, "mqtt_msg_process_publish(): unhandle recv'ed", data, len);
	}
	}
	return 0;
}

void *_mqtt_async_handler(void *arg) {
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT *)arg;
	int ret = 0;
	char *data = NULL;
	int data_len;
	int thread_id;

	if (_thread_id(&thread_id) == 0) {
		client->thread_id = thread_id;
	}
	// publish receiver
	// error detection
	// keep alive handling
	// reconnect logic

	while (client->state != MQTT_ASYNC_CLIENT_STATE_DELETING && client->state != MQTT_ASYNC_CLIENT_STATE_DISABLED) {
		if (client->state == MQTT_ASYNC_CLIENT_STATE_CONNECTED) {
			if ((ret = mqtt_recv(client->mqtt, &data, &data_len, client->timeout_in_ms)) != 0) {
				if (ret != ERR_TR50_TCP_TIMEOUT) {
					log_important_info("_mqtt_async_handler(): mqtt_recv failed [%d]", ret);
					_tr50_mutex_lock(client->mux);
					if (client->state == MQTT_ASYNC_CLIENT_STATE_CONNECTED) {
						_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_CONNECTED, MQTT_ASYNC_CLIENT_STATE_BROKEN, ret, "MQTT Receive failed.");
					}
					_tr50_mutex_unlock(client->mux);
				}
			} else {
				log_debug("_mqtt_async_handler(): mqtt_recv OK");
				_mqtt_async_handler_process(client, data, data_len);
				_memory_free(data);
			}
		}
		log_recurring(LOG_TYPE_IMPORTANT_INFO, __FILE__, __LINE__, 60, 1, "_mqtt_async_handler ... state[%d]", client->state);

		if (mqtt_qos_any_expired(client->qos)) {
			log_important_info("_mqtt_async_handler(): mqtt_qos_check failed [%d]", ret);
			_tr50_mutex_lock(client->mux);
			if (client->state == MQTT_ASYNC_CLIENT_STATE_CONNECTED) {
				_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_CONNECTED, MQTT_ASYNC_CLIENT_STATE_BROKEN, ret, "MQTT Ack not recevied.");
			}
			_tr50_mutex_unlock(client->mux);
			mqtt_qos_clear(client->qos, ERR_MQTT_QOS_FAILURE);
		}

		_mqtt_async_handler_keepalive(client);
		_mqtt_async_handler_reconnect(client);
	}
	mqtt_qos_clear(client->qos, ERR_MQTT_QOS_STOPPING);
	log_important_info("_mqtt_async_handler(): returned.");
	return NULL;
}

int _mqtt_async_publish_base(void *async_client, const char *topic, const char *data, int len, int retain, int qos, mqtt_qos_callback callback, void *callback_custom) {
	int ret, thread_id;
	unsigned short msg_id;
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT *)async_client;

	if (client == NULL) {
		return ERR_TR50_ASYNC_CLIENT_NOT_CONNECTED;
	}

	if (client->check_thread_id && _thread_id(&thread_id) == 0 && thread_id == client->thread_id) {
		log_should_not_happen("mqtt_async_publish(): cannot publish on this callback[%s]!", data);
		return ERR_TR50_ASYNC_CLIENT_SAME_THREAD;
	}

	_tr50_mutex_lock(client->mux);

	if (client->state != MQTT_ASYNC_CLIENT_STATE_CONNECTED || !client->mqtt) {
		_tr50_mutex_unlock(client->mux);
		return ERR_TR50_ASYNC_CLIENT_NOT_CONNECTED;
	}
	
	msg_id = client->msg_id_seq++;

	if (qos == 1) {
		if ((ret = mqtt_qos_add(client->qos, msg_id, callback, callback_custom)) != 0) {
			_tr50_mutex_unlock(client->mux);
			return ret;
		}
	}

	if ((ret = mqtt_publish(client->mqtt, qos, retain, msg_id, topic, data, len)) != 0) {
		_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_CONNECTED, MQTT_ASYNC_CLIENT_STATE_BROKEN, ret, "MQTT Publish failed.");
	}
	_tr50_mutex_unlock(client->mux);
	return ret;
}

int mqtt_async_publish(void *async_client, const char *topic, const char *data, int len, int retain) {
	return _mqtt_async_publish_base(async_client, topic, data, len, retain, MQTT_PAYLOAD_QOS_0, NULL, NULL);
}

int mqtt_async_publish_qos1(void *async_client, const char *topic, const char *data, int len, int retain, mqtt_qos_callback callback, void *callback_custom) {
	return _mqtt_async_publish_base(async_client, topic, data, len, retain, MQTT_PAYLOAD_QOS_1, callback, callback_custom);
}

int mqtt_async_subscribe(void *async_client, const char *topic, int qos, mqtt_qos_callback callback, void *callback_custom) {
	int ret, thread_id;
	unsigned short msg_id;

	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT*)async_client;

	if (client == NULL) return ERR_TR50_ASYNC_CLIENT_NOT_CONNECTED;

	if (client->check_thread_id && _thread_id(&thread_id) == 0 && thread_id == client->thread_id) {
		log_should_not_happen("mqtt_async_subscribe(): cannot subscribe on this callback!");
		return ERR_TR50_ASYNC_CLIENT_SAME_THREAD;
	}

	_tr50_mutex_lock(client->mux);

	if (client->state != MQTT_ASYNC_CLIENT_STATE_CONNECTED || !client->mqtt) {
		_tr50_mutex_unlock(client->mux);
		return ERR_TR50_ASYNC_CLIENT_NOT_CONNECTED;
	}

	msg_id = client->msg_id_seq++;

	if ((ret = mqtt_qos_add(client->qos, msg_id, callback, callback_custom)) != 0) {
		_tr50_mutex_unlock(client->mux);
		return ret;
	}

	if ((ret = mqtt_subscribe(client->mqtt, msg_id, topic, qos)) != 0) {
		_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_CONNECTED, MQTT_ASYNC_CLIENT_STATE_BROKEN, ret, "MQTT Publish failed.");
	}
	_tr50_mutex_unlock(client->mux);
	return ret;
}

int mqtt_async_unsubscribe(void *async_client, const char *topic, mqtt_qos_callback callback, void *callback_custom) {
	int ret, thread_id;
	unsigned short msg_id;
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT*)async_client;

	if (client == NULL) return ERR_TR50_ASYNC_CLIENT_NOT_CONNECTED;

	if (client->check_thread_id && _thread_id(&thread_id) == 0 && thread_id == client->thread_id) {
		log_should_not_happen("mqtt_async_subscribe(): cannot subscribe on this callback!");
		return ERR_TR50_ASYNC_CLIENT_SAME_THREAD;
	}

	_tr50_mutex_lock(client->mux);

	if (client->state != MQTT_ASYNC_CLIENT_STATE_CONNECTED || !client->mqtt) {
		_tr50_mutex_unlock(client->mux);
		return ERR_TR50_ASYNC_CLIENT_NOT_CONNECTED;
	}

	msg_id = client->msg_id_seq++;
	
	if ((ret = mqtt_qos_add(client->qos, msg_id, callback, callback_custom)) != 0) {
		_tr50_mutex_unlock(client->mux);
		return ret;
	}

	if ((ret = mqtt_unsubscribe(client->mqtt, msg_id, topic)) != 0) {
		_mqtt_async_state_change(client, MQTT_ASYNC_CLIENT_STATE_CONNECTED, MQTT_ASYNC_CLIENT_STATE_BROKEN, ret, "MQTT Publish failed.");
	}
	_tr50_mutex_unlock(client->mux);
	return ret;
}

int mqtt_async_state(void *async_client) {
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT *)async_client;
	return client->state;
}

void *mqtt_async_base_handle(void *async_client) {
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT *)async_client;
	return client->mqtt;
}

void _mqtt_async_state_change(_MQTT_ASYNC_CLIENT *client, int old_state, int new_state, int error, const char *why) {
	client->state = new_state;
	client->check_thread_id = 1;
	if (client->state_change_callback) {
		client->state_change_callback(old_state, new_state, error, why, client->callback_custom);
	}
	client->check_thread_id = 0;
	log_important_info("_mqtt_async_state_change(): from [%d] to [%d] due to [%s(%d)]", old_state, new_state, why, error);
}

int mqtt_async_reconnect_count(void *async_client) {
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT *)async_client;
	return client->stats_reconnect_count;
}

int mqtt_async_reconnect_attempt_count(void *async_client) {
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT *)async_client;
	return client->stats_reconnect_attempt_count;
}

long long mqtt_async_stats_byte_sent(void *async_client) {
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT *)async_client;
	if (client->mqtt == NULL) {
		return client->stats_total_byte_sent;
	}
	return client->stats_total_byte_sent + mqtt_stats_byte_sent(client->mqtt);
}

long long mqtt_async_stats_byte_recv(void *async_client) {
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT *)async_client;
	if (client->mqtt == NULL) {
		return client->stats_total_byte_recv;
	}
	return client->stats_total_byte_recv + mqtt_stats_byte_recv(client->mqtt);
}

void mqtt_async_stats_clear(void *async_client) {
	_MQTT_ASYNC_CLIENT *client = (_MQTT_ASYNC_CLIENT *)async_client;
	if (client == NULL) {
		return;
	}
	client->stats_total_byte_recv = 0;
	client->stats_total_byte_sent = 0;
	if (client->mqtt) {
		mqtt_stats_clear(client->mqtt);
	}
}
