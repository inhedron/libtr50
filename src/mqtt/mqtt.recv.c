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

#include <time.h>

#include <tr50/mqtt/mqtt.h>

#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/tcp.h>
#include <tr50/util/time.h>

#define MQTT_DEFAULT_RECV_BUF_LEN	128

int mqtt_send(void *mqtt_handle, const char *buf, int len, int timeout) {
	_MQTT_CLIENT *client = (_MQTT_CLIENT *)mqtt_handle;
	int ret;
	if ((ret = _tcp_send(client->sock, buf, len, timeout)) == 0) {
		client->byte_sent += len;
	}

	return 0;
}

int mqtt_recv(void *mqtt_handle, char **data, int *len, int timeout) {
	_MQTT_CLIENT *client = (_MQTT_CLIENT *)mqtt_handle;
	char *buffer;
	int ret;
	int buffer_max_len = 0;
	int buffer_cur_len = 0;
	int remaining_len = 0;
	int read_len = 2; // fixed header
	int recv_len;
	int encoded_size;
	long long starting_time = _time_now();
	long long wait_time;

	if ((buffer = (char *)_memory_malloc(MQTT_DEFAULT_RECV_BUF_LEN)) == NULL) {
		return ERR_TR50_MALLOC;
	}
	buffer_max_len = MQTT_DEFAULT_RECV_BUF_LEN;

	//// reading fixed header
	while (read_len > 0) {

		if ((wait_time = starting_time + timeout - _time_now()) < 0) {  // checking for timeout
			log_important_info("mqtt_recv(): wait_time[%lld] starting_time[%lld] timeout[%d]", wait_time, starting_time, timeout);
			_memory_free(buffer);
			return ERR_TR50_TIMEOUT;
		}

		if (read_len > buffer_max_len - buffer_cur_len) { // resize buffer if necessary
			if ((buffer = (char *)_memory_realloc(buffer, buffer_cur_len + read_len)) == NULL) {
				return ERR_TR50_MALLOC;
			}
			buffer_max_len = buffer_cur_len + read_len;
		}

		if ((ret = _tcp_recv(client->sock, buffer + buffer_cur_len, &read_len, (int)wait_time)) < 0) {
			_memory_free(buffer);
			if (buffer_cur_len > 0 && ret == ERR_TR50_TIMEOUT) { // timeout reading the remaining packet, we should treat it as hard error
				return ERR_TR50_TIMEOUT;
			}
			return ret;
		}

		if (buffer_cur_len == 0) { // reset starting time if we recv the first byte.
			starting_time = _time_now();
		}

		client->byte_recv += read_len;

		buffer_cur_len += read_len;

		if (buffer_cur_len > 1) {
			char *ptr = buffer + buffer_cur_len - 1;
			if (!(*ptr & 128)) {
				break;
			}
		}
		read_len = 1;
	}

	/// reading remaining variable header and payload
	if (_mqtt_decode_header_length(buffer + 1, buffer_cur_len, &encoded_size, &remaining_len) < 0) {
		_memory_free(buffer);
		return ERR_TR50_MQTT_RECV_DECODE_HDR_LEN;
	}

	read_len = remaining_len;
	if (read_len > buffer_max_len - buffer_cur_len) { // resize buffer if necessary
		if ((buffer = (char *)_memory_realloc(buffer, buffer_cur_len + read_len)) == NULL) {
			return ERR_TR50_MALLOC;
		}
		buffer_max_len = buffer_cur_len + read_len;
	}

	if ((wait_time = starting_time + timeout - _time_now()) < 0) {
		wait_time = 0;
	}

	while (read_len > 0) {
		recv_len = read_len;
		if ((ret = _tcp_recv(client->sock, buffer + buffer_cur_len, &recv_len, (int)wait_time)) < 0) {
			_memory_free(buffer);
			return ret;
		}
		client->byte_recv += recv_len;

		buffer_cur_len += recv_len;
		read_len -= recv_len;
	}

	*data = buffer;
	*len = buffer_cur_len;
	client->last_recv = _time_now();
	return 0;
}
