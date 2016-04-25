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

#include <tr50/internal/tr50.h>

#include <tr50/util/log.h>
#include <tr50/util/mutex.h>
#include <tr50/util/thread.h>
#include <tr50/util/time.h>

#define ERR_TR50_DICTIONARY_ITEM_EXISTS		-1
#define ERR_TR50_DICTIONARY_ITEM_NOTFOUND	-2

#define TR50_PENDING_TIME_SHIFT_ALLOW			3600000
#define TR50_PENDING_EXPIRATION_CHECK_INTERVAL	1000

void *_tr50_pending_expiration_handler(void *client);


int tr50_pending_create(_TR50_CLIENT *client) {
	_TR50_PENDING *pending = &client->pending;
	_tr50_mutex_create(&pending->mux);
	_thread_create(&pending->thread, "TR50:Pending", _tr50_pending_expiration_handler, client);
	return 0;
}

int tr50_pending_delete(_TR50_CLIENT *client) {
	_TR50_PENDING *pending = &client->pending;
	pending->is_deleting = 1;
	_thread_join(pending->thread);
	_thread_delete(pending->thread);
	_tr50_mutex_delete(pending->mux);
	return 0;
}

int tr50_pending_add(_TR50_CLIENT *client, _TR50_MESSAGE *message) {
	_TR50_PENDING *pending = &client->pending;
	_TR50_MESSAGE *tail;
	message->pending_sent_timestamp = _time_now();
	message->pending_expiration_timestamp = message->pending_sent_timestamp + message->callback_timeout;

	_tr50_mutex_lock(pending->mux);

	if ((tail = pending->list_tail) == NULL) {
		pending->list_head = message;
		pending->list_tail = message;
	} else {
		tail->pending_next = message;
		message->pending_previous = tail;
		pending->list_tail = message;
	}
	++pending->count;
	_tr50_mutex_unlock(pending->mux);
	return 0;
}

void _tr50_pending_linklist_remove(_TR50_CLIENT *client, _TR50_MESSAGE *message) {
	_TR50_PENDING *pending = &client->pending;
	_TR50_MESSAGE *previous = (_TR50_MESSAGE *)message->pending_previous;
	_TR50_MESSAGE *next = (_TR50_MESSAGE *)message->pending_next;

	if (previous != NULL) {
		previous->pending_next = next;
	} else {
		pending->list_head = next;
	}

	if (next != NULL) {
		next->pending_previous = previous;
	} else {
		pending->list_tail = previous;
	}
	--pending->count;
}

_TR50_MESSAGE *tr50_pending_find_and_remove(_TR50_CLIENT *client, int hash) {
	_TR50_PENDING *pending = &client->pending;
	_TR50_MESSAGE *ptr;

	_tr50_mutex_lock(pending->mux);
	ptr = (_TR50_MESSAGE *)pending->list_head;

	while (ptr) {
		if (ptr->seq_id == hash) {
			_tr50_pending_linklist_remove(client, ptr);
			_tr50_mutex_unlock(pending->mux);
			return ptr;
		}
		ptr = ptr->pending_next;
	}
	_tr50_mutex_unlock(pending->mux);
	return NULL;
}

void *_tr50_pending_expiration_handler(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	_TR50_PENDING *pending = &client->pending;

	while (!pending->is_deleting) {
		long long now = _time_now();
		_TR50_MESSAGE *next;

		_tr50_mutex_lock(pending->mux);
		next = (_TR50_MESSAGE *)pending->list_head;
		while (next) {
			_TR50_MESSAGE *message = next;
			next = (_TR50_MESSAGE *)message->pending_next; // set next first coz it might be removed below.

			if (message->pending_expiration_timestamp <= now) {
				if (message->pending_expiration_timestamp - now > TR50_PENDING_TIME_SHIFT_ALLOW) { // if time shifted forward
					log_important_info("message rescheduled expiration: expiration - now = [%d]", (int)(message->pending_expiration_timestamp - now));
					message->pending_expiration_timestamp = now + message->callback_timeout;
				} else {
					++pending->expired_count;
					_tr50_pending_linklist_remove(client, message);

					if (message->message_type == TR50_MESSAGE_TYPE_OBJ && message->reply_callback) {
						((tr50_async_reply_callback)message->reply_callback)(tr50, ERR_TR50_REQ_TIMEOUT, message, NULL, message->callback_custom);
					} else if (message->message_type == TR50_MESSAGE_TYPE_RAW && message->raw_callback) {
						((tr50_async_raw_reply_callback)message->raw_callback)(ERR_TR50_REQ_TIMEOUT, NULL, message->callback_custom);
					}

					log_debug("message seq_id[%d] expired.", message->seq_id);
					tr50_message_delete(message);
				}
			} else if (now - message->pending_expiration_timestamp > TR50_PENDING_TIME_SHIFT_ALLOW + message->callback_timeout) { // if time shifted backward
				log_important_info("message rescheduled expiration: expiration - now = [%d]", (int)(message->pending_expiration_timestamp - now));
				message->pending_expiration_timestamp = now + message->callback_timeout;
			}

		}
		_tr50_mutex_unlock(pending->mux);
		_thread_sleep(TR50_PENDING_EXPIRATION_CHECK_INTERVAL);
	}
	return NULL;
}

int tr50_pending_count(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	return client->pending.count;
}

int tr50_pending_expired_count(void *tr50) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	return client->pending.expired_count;
}

