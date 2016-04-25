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

#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/time.h>

int method_registration_inuse = FALSE;
int tr50_is_method_registered() {
	return method_registration_inuse;
}
int tr50_method_register(void *tr50, const char *method_name, tr50_method_callback func) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	_TR50_METHOD *method = (_TR50_METHOD*)_memory_malloc(sizeof(_TR50_METHOD));
	_memory_memset(method, 0, sizeof(_TR50_METHOD));
	method_registration_inuse = TRUE;
	method->callback = func;
	strncpy(method->name, method_name, TR50_METHOD_NAME_LEN);
	method->next = client->method_head;
	client->method_head = method;
	log_important_info("tr50_method_register(): method[%s] registered.", method->name);
	return 0;
}
 

int tr50_method_process(void *tr50, const char *id, const char *key, const char *method_name, const char *from, JSON *params, void* custom) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	int ret;
	_TR50_METHOD *method = client->method_head;
	if (!method_registration_inuse) return 0;
	if (!method_name) {
		return ERR_TR50_METHOD_UNKNOWN;
	}

	while (method) {
		if (strcmp(method->name, method_name) == 0) {
			long long ended, started;
			log_debug("tr50_method_process(): executing method[%s] ...", method->name);
			started = _time_now();
			if ((ret = method->callback(tr50, id, key, method_name, from, params, NULL)) != 0) {
				log_important_info("tr50_method_process(): cmd[%s] failed [%d]", method_name, ret);
			}
			ended = _time_now();
			if (ended - started > 1000) {
				log_need_investigation("tr50_method_process(): method callback for [%s] is taking [%d]ms.", method->name, ended - started);
			}
			return ret;
		}
		method = (_TR50_METHOD *)method->next;
	}
	log_need_investigation("tr50_method_process(): unknown method[%s] recv'ed", method_name);
	tr50_method_ack(tr50, id, ERR_TR50_METHOD_UNKNOWN, "Unknown method", NULL);
	return ERR_TR50_METHOD_UNKNOWN;
} 
int tr50_method_update(void *tr50, const char *id, const char *msg) {
	void *message;
	JSON *params = tr50_json_create_object();

	tr50_json_add_string_to_object(params, "id", id);
	if (msg != NULL) {
		tr50_json_add_string_to_object(params, "msg", msg);
	}

	tr50_message_create(&message);
	tr50_message_add_command(message,"1","mailbox.update", params);
	if (tr50_api_call(tr50, message) != 0) {
		tr50_message_delete(message);
	}
	return 0;
}

int tr50_method_ack(void *tr50, const char *id, int status, const char *err_message, JSON *ack_params) {
	int ret;
	void *message;
	JSON *params = tr50_json_create_object();

	tr50_json_add_string_to_object(params, "id", id);

	if (status != 0) {
		tr50_json_add_number_to_object(params, "errorCode", status);
		if (err_message) {
			tr50_json_add_string_to_object(params, "errorMessage", err_message);
		}
	}
	if (ack_params) {
		tr50_json_add_item_to_object(params, "params", ack_params);
	}

	tr50_message_create(&message);
	tr50_message_add_command(message,"1","mailbox.ack", params);
	if ((ret = tr50_api_call(tr50, message)) != 0) {
		tr50_message_delete(message);
	}
	return ret;
}
