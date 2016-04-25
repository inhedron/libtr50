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
#include <string.h>

#include <tr50/internal/tr50.h>

#include <tr50/mqtt/mqtt.h>

#include <tr50/tr50.h>

#include <tr50/util/compress.h>
#include <tr50/util/event.h>
#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/mutex.h>
#include <tr50/util/platform.h>

#define TR50_MAX_ID					65536
#define TR50_MIN_COMPRESSION_LEN	128

int _tr50_build_payload(_TR50_CLIENT *client, const char **topic, _TR50_MESSAGE *message, char **data, int *data_len);

int tr50_api_msg_id_next(void *tr50) {
	int id;
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;

	_tr50_mutex_lock(client->mux);
	if (client->seq_id == TR50_MAX_ID) {
		client->seq_id = 0;
	}
	id = ++client->seq_id;
	_tr50_mutex_unlock(client->mux);
	return id;
}

int tr50_api_call(void *tr50, void *message) {
	int id;
	return tr50_api_call_async(tr50, message, &id, NULL, NULL, 30000);
}

int tr50_api_call_async(void *tr50, void *message, int *seq_id, tr50_async_reply_callback reply_callback, void *custom, int timeout) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	_TR50_MESSAGE *msg = (_TR50_MESSAGE *)message;

	const char *topic = client->compress ? TR50_TOPIC_COMPRESS_API : TR50_TOPIC_API;
	char topic_with_seq[64];
	char *data = NULL;
	int data_len, ret, local_seq_id;

	_tr50_mutex_lock(client->mux);
	client->stats.in_api_call_async = 1;

	if (client->state == TR50_STATE_STOPPED) {
		ret = ERR_TR50_NOT_CONNECTED;
		goto end_error;
	}
	if (client->is_stopping) {
		ret = ERR_TR50_STOPPING;
		goto end_error;
	}

	if (client->seq_id == TR50_MAX_ID) {
		client->seq_id = 0;
	}
	local_seq_id = ++client->seq_id;
	if (seq_id) {
		*seq_id = local_seq_id;
	}

	msg->seq_id = local_seq_id;
	msg->message_type = TR50_MESSAGE_TYPE_OBJ;
	msg->reply_callback = (void *)reply_callback;
	msg->callback_custom = custom;
	msg->callback_timeout = timeout;

	if ((ret = _tr50_build_payload(client, &topic, message, &data, &data_len)) != 0) {
		goto end_error;
	}

	snprintf(topic_with_seq, 63, "%s/%d", topic, msg->seq_id);

	if ((ret = tr50_pending_add(client, msg)) != 0) {
		goto end_error;
	}

	client->stats.in_api_call_async = 2;
	if ((ret = mqtt_async_publish(client->mqtt, topic_with_seq, data, data_len, 0)) != 0) {
		if ((msg = tr50_pending_find_and_remove(client, local_seq_id)) != NULL) {
			goto end_error;
		}
		// already expirated, return okay.
		client->stats.in_api_call_async = 0;
		_tr50_mutex_unlock(client->mux);
	} else {
		client->stats.in_api_call_async = 0;
		_tr50_mutex_unlock(client->mux);
		_tr50_stats_pub_sent_up(client, data_len);
	}
	_memory_free(data);
	return 0;
end_error:
	client->stats.in_api_call_async = 0;
	_tr50_mutex_unlock(client->mux);
	if (data) {
		_memory_free(data);
	}
	return ret;
}

int _tr50_build_payload(_TR50_CLIENT *client, const char **topic, _TR50_MESSAGE *message, char **data, int *data_len) {
	int ret = 0;
	if (client->compress) {
		char *raw;
		int raw_len;

		if ((ret = tr50_message_to_string(*topic, message, &raw, &raw_len)) != 0) {
			return ret;
		}
		if (client->config.api_watcher_handler) {
			client->config.api_watcher_handler(raw, raw_len, 0);
		}
		if (raw_len <= TR50_MIN_COMPRESSION_LEN) {
			*data_len = raw_len;
			*data = raw;
			*topic = TR50_TOPIC_API;
			return ret;
		}
		if ((ret = _compress_deflate(raw, raw_len, data, data_len)) != 0) { // if compression failed, send without compression
			log_need_investigation("_compress_deflate(): failed [%d]", ret);
			*data = raw;
			*data_len = raw_len;
			*topic = TR50_TOPIC_API;
		} else {
			_tr50_stats_set_compress_ratio(client, raw_len, *data_len);
			_memory_free(raw);
		}
	} else {
		if ((ret = tr50_message_to_string(*topic, message, data, data_len)) != 0) {
			return ret;
		}
		if (client->config.api_watcher_handler) {
			client->config.api_watcher_handler(*data, *data_len, 0);
		}
	}
	return ret;
}

int tr50_api_raw_async(void *tr50, const char *request_json, int *seq_id, tr50_async_raw_reply_callback reply_callback, void *custom, int timeout) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	_TR50_MESSAGE *msg = NULL;

	char topic_with_seq[64];
	int request_len, ret, local_seq_id;

	_tr50_mutex_lock(client->mux);
	client->stats.in_api_raw_async = 1;

	if (client->state == TR50_STATE_STOPPED) {
		ret = ERR_TR50_NOT_CONNECTED;
		goto end_error;
	}
	if (client->is_stopping) {
		ret = ERR_TR50_STOPPING;
		goto end_error;
	}

	if (client->seq_id == TR50_MAX_ID) {
		client->seq_id = 0;
	}
	local_seq_id = ++client->seq_id;
	if (seq_id) {
		*seq_id = local_seq_id;
	}
	request_len = strlen(request_json);

	tr50_message_create((void *)&msg);
	msg->seq_id = local_seq_id;
	msg->message_type = TR50_MESSAGE_TYPE_RAW;
	msg->raw_callback = (void *)reply_callback;
	msg->callback_custom = custom;
	msg->callback_timeout = timeout;

	if ((ret = tr50_pending_add(client, msg)) != 0) {
		goto end_error;
	}

	if (client->config.api_watcher_handler) {
		client->config.api_watcher_handler(request_json, request_len, 0);
	}

	client->stats.in_api_raw_async = 2;
	if (client->compress && request_len >= TR50_MIN_COMPRESSION_LEN) {
		int out_len;
		char *out;

		snprintf(topic_with_seq, 63, "%s/%d", TR50_TOPIC_COMPRESS_API, msg->seq_id);

		if ((ret = _compress_deflate(request_json, request_len, &out, &out_len)) != 0) { // if compression failed, send without compression
			log_need_investigation("_compress_deflate(): failed [%d]", ret);
			goto end_error;
		}
		_tr50_stats_set_compress_ratio(client, request_len, out_len);

		if ((ret = mqtt_async_publish(client->mqtt, topic_with_seq, out, out_len, 0)) != 0) {
			if ((msg = tr50_pending_find_and_remove(client, local_seq_id)) != NULL) {
				_memory_free(out);
				goto end_error;
			}
			// already expirated, return okay.
		}
		_memory_free(out);
	} else {
		snprintf(topic_with_seq, 63, "%s/%d", TR50_TOPIC_API, msg->seq_id);

		if ((ret = mqtt_async_publish(client->mqtt, topic_with_seq, request_json, request_len, 0)) != 0) {
			if ((msg = tr50_pending_find_and_remove(client, local_seq_id)) != NULL) {
				goto end_error;
			}
			// already expirated, return okay.
		}
	}
	_tr50_stats_pub_sent_up(client, request_len);
	client->stats.in_api_raw_async = 0;
	_tr50_mutex_unlock(client->mux);
	return 0;
end_error:
	client->stats.in_api_raw_async = 0;
	_tr50_mutex_unlock(client->mux);
	if (msg) {
		tr50_message_delete(msg);
	}
	return ret;
}

typedef struct {
	void *evt;
	int status;
	void *reply;
} _TR50_SYNC_OBJ;

void _tr50_api_call_sync_callback(void *tr50, int status, const void *request_message, void *message, void *custom) {
	_TR50_SYNC_OBJ *sync = (_TR50_SYNC_OBJ *)custom;
	if (status == 0) {
		sync->reply = message;
	}
	sync->status = status;
	_tr50_event_signal(sync->evt);
}

int tr50_api_call_sync(void *tr50, void *message, void **reply_message, int timeout) {
	int ret, id;
	_TR50_SYNC_OBJ sync;

	_memory_memset(&sync, 0, sizeof(_TR50_SYNC_OBJ));
	if ((ret = _tr50_event_create(&sync.evt)) != 0) {
		return ret;
	}
	sync.status = -1;

	if ((ret = tr50_api_call_async(tr50, message, &id, _tr50_api_call_sync_callback, &sync, timeout)) != 0) {
		_tr50_event_delete(sync.evt);
		return ret;
	}
	_tr50_event_wait(sync.evt);

	if (sync.status == 0) {
		*reply_message = sync.reply;
	} else {
		const char *err_message = "TR50: Operation Timeout.";
		// since the message will expire, we are creating an error reply and return 0, so that the caller does not free the request.
		tr50_message_create(reply_message);
		((_TR50_MESSAGE *)*reply_message)->is_reply=TRUE;
		tr50_json_add_false_to_object(((_TR50_MESSAGE *)*reply_message)->json, "success");
		tr50_json_add_item_to_object(((_TR50_MESSAGE *)*reply_message)->json, "errorcodes", tr50_json_create_int_array(&sync.status, 1));
		tr50_json_add_item_to_object(((_TR50_MESSAGE *)*reply_message)->json, "errormessages", tr50_json_create_string_array(&err_message, 1));
	}
	_tr50_event_delete(sync.evt);
	return 0;
}

void _tr50_api_raw_sync_callback(int status, const char *reply_json, void *custom) {
	_TR50_SYNC_OBJ *sync = (_TR50_SYNC_OBJ *)custom;
	if (status == 0) {
		sync->reply = _memory_clone((void *)reply_json, strlen(reply_json));
	}
	sync->status = status;
	_tr50_event_signal(sync->evt);
}

int tr50_api_raw_sync(void *tr50, const char *request_json, char **reply_json, int timeout) {
	int ret, id;
	_TR50_SYNC_OBJ sync;

	_memory_memset(&sync, 0, sizeof(_TR50_SYNC_OBJ));
	if ((ret = _tr50_event_create(&sync.evt)) != 0) {
		return ret;
	}
	sync.status = -1;

	if ((ret = tr50_api_raw_async(tr50, request_json, &id, _tr50_api_raw_sync_callback, &sync, timeout)) != 0) {
		_tr50_event_delete(sync.evt);
		return ret;
	}
	_tr50_event_wait(sync.evt);

	if (sync.status == 0) {
		*reply_json = sync.reply;
	}
	_tr50_event_delete(sync.evt);
	return sync.status;
}
