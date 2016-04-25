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

#include <tr50/internal/tr50.h>

#include <tr50/mqtt/mqtt.h>

#include <tr50/util/memory.h>
#include <tr50/util/mutex.h>
#include <tr50/util/time.h>

void _tr50_stats_pub_recv_up(_TR50_CLIENT *client, int byte_recv) {
	_TR50_STATS *stats = &client->stats;
	_tr50_mutex_lock(stats->mux);
	stats->pub_byte_recv += byte_recv;
	++stats->pub_recv;
	_tr50_mutex_unlock(stats->mux);
}

void _tr50_stats_pub_sent_up(_TR50_CLIENT *client, int byte_sent) {
	_TR50_STATS *stats = &client->stats;
	_tr50_mutex_lock(stats->mux);
	stats->pub_byte_sent += byte_sent;
	++stats->pub_sent;
	_tr50_mutex_unlock(stats->mux);
}

void _tr50_stats_notify_up(_TR50_CLIENT *client) {
	_TR50_STATS *stats = &client->stats;
	_tr50_mutex_lock(stats->mux);
	++stats->notify_count;
	_tr50_mutex_unlock(stats->mux);
}

void _tr50_stats_mailbox_check_up(_TR50_CLIENT *client) {
	_TR50_STATS *stats = &client->stats;
	_tr50_mutex_lock(stats->mux);
	++stats->mailbox_check_count;
	_tr50_mutex_unlock(stats->mux);
}

void _tr50_stats_set_connected(_TR50_CLIENT *client) {
	_TR50_STATS *stats = &client->stats;
	_tr50_mutex_lock(stats->mux);
	stats->connected_timestamp = _time_now();
	++stats->connected_count;
	_tr50_mutex_unlock(stats->mux);
}

void _tr50_stats_set_compress_ratio(_TR50_CLIENT *client, int data_len, int compressed_len) {
	_TR50_STATS *stats = &client->stats;
	_tr50_mutex_lock(stats->mux);
	stats->compression_compressed_len[stats->compression_ratio_count % _TR50_COMPRESSION_HISTORY_MAX] = compressed_len;
	stats->compression_original_len[stats->compression_ratio_count % _TR50_COMPRESSION_HISTORY_MAX] = data_len;
	++stats->compression_ratio_count;
	_tr50_mutex_unlock(stats->mux);
}

void _tr50_stats_set_last_error(_TR50_CLIENT *client, int error, const char *error_message) {
	_TR50_STATS *stats = &client->stats;
	if (error == 0) {
		return;
	}
	_tr50_mutex_lock(stats->mux);
	stats->last_error = error;
	strncpy(stats->last_error_message, error_message, _TR50_STATS_LAST_ERROR_MESSAGE_LEN);
	stats->last_error_timestamp = _time_now();
	_tr50_mutex_unlock(stats->mux);
}

int tr50_stats_pub_recv(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	return client->stats.pub_recv;
}
int tr50_stats_pub_sent(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	return client->stats.pub_sent;
}

int tr50_stats_byte_recv(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	if (client->mqtt) {
		return client->stats.total_recv + mqtt_async_stats_byte_recv(client->mqtt);
	}
	return client->stats.total_recv;
}

int tr50_stats_byte_sent(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	if (client->mqtt) {
		return client->stats.total_sent + mqtt_async_stats_byte_sent(client->mqtt);
	}
	return client->stats.total_sent;
}

int tr50_stats_compress_ratio(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	int i;
	int count;
	int compressed_sum = 0;
	int uncompressed_sum = 0;
	int ratio = 0;

	count = client->stats.compression_ratio_count;

	if (count == 0) {
		return 0;
	}

	if (count > 100) {
		for (i = 0; i < 100; ++i) {
			compressed_sum += client->stats.compression_compressed_len[i];
			uncompressed_sum += client->stats.compression_original_len[i];
		}
	} else {
		for (i = 0; i < count; ++i) {
			compressed_sum += client->stats.compression_compressed_len[i];
			uncompressed_sum += client->stats.compression_original_len[i];
		}
	}
	ratio = 100 - (100 * compressed_sum) / uncompressed_sum;
	return ratio;
}

void tr50_stats_clear_compression_ratio(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	_TR50_STATS *stats;

	if (client == NULL) {
		return;
	}
	stats = &client->stats;
	_tr50_mutex_lock(stats->mux);
	client->stats.compression_ratio_count = 0;
	_memory_memset(client->stats.compression_compressed_len, 0, _TR50_COMPRESSION_HISTORY_MAX);
	_tr50_mutex_unlock(stats->mux);
}

void tr50_stats_clear_send_recv(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	_TR50_STATS *stats;

	if (client == NULL) {
		return;
	}
	stats = &client->stats;
	_tr50_mutex_lock(stats->mux);
	if (client->mqtt) {
		mqtt_async_stats_clear(client->mqtt);
	}
	stats->pub_byte_sent = 0;
	stats->pub_sent = 0;
	stats->total_sent = 0;

	stats->pub_byte_recv = 0;
	stats->pub_recv = 0;
	stats->total_recv = 0;

	_tr50_mutex_unlock(stats->mux);
}

void tr50_stats_clear_last_error(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	_TR50_STATS *stats;

	if (client == NULL) {
		return;
	}
	stats = &client->stats;
	_tr50_mutex_lock(stats->mux);
	stats->last_error = 0;
	_memory_memset(stats->last_error_message, 0, _TR50_STATS_LAST_ERROR_MESSAGE_LEN + 1);
	stats->last_error_timestamp = 0;
	_tr50_mutex_unlock(stats->mux);
}

long long tr50_stats_last_connected(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	return client->stats.connected_timestamp;
}

int tr50_stats_reconnect_attempt_count(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	if (client->mqtt) {
		return mqtt_async_reconnect_attempt_count(client->mqtt);
	}
	return 0;
}

int tr50_stats_reconnect_count(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	if (client->mqtt) {
		return mqtt_async_reconnect_count(client->mqtt);
	}
	return 0;
}

int tr50_stats_last_error(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	return client->stats.last_error;
}

int tr50_stats_notify(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	return client->stats.notify_count;
}

int tr50_stats_mailbox_check(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	return client->stats.mailbox_check_count;
}

int tr50_stats_in_api_call_async(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT*)tr50;
	return client->stats.in_api_call_async;
}

int tr50_stats_in_api_raw_async(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT*)tr50;
	return client->stats.in_api_raw_async;
}
