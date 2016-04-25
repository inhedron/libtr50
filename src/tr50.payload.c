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
#include <stdlib.h>
#include <string.h>
#include <tr50/util/json.h>
#include <tr50/internal/tr50.h>
#include <tr50/tr50.h>
#include <tr50/util/log.h>
#include <tr50/util/memory.h>

int tr50_message_to_string(const char *topic, void *tr50_message, char **data, int *data_len) {
	_TR50_MESSAGE *message = (_TR50_MESSAGE *)tr50_message;

	if (!(*data = tr50_json_print(message->json))) {
		return ERR_TR50_MALLOC;
	}
	*data_len = strlen(*data);
	return 0;
}

int tr50_message_from_string(const char *payload, int payload_len, void **tr50_message) {
	JSON *body, *json;
	const char *command;
	_TR50_MESSAGE *message;
	int ret = 0;

	if (!(json = tr50_json_parse(payload))) {
		return ERR_TR50_JSON_INVALID;
	}

	if ((body = tr50_json_get_array_item(json, 0)) == NULL) {
		log_need_investigation("invalid tr50 json: %s", payload);
	}
	ret = tr50_message_create((void *)&message);
	tr50_json_delete(message->json);
	message->json = json;
	if ((command = tr50_json_get_object_item_as_string(body, "command")) == NULL) {
		message->is_reply = TRUE;
	}
	
	if (ret == 0) {
		*tr50_message = message;
	}

	return ret;
}
