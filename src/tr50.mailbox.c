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

#include <tr50/internal/tr50.h>
#include <string.h>
#include <tr50/util/log.h>
#include <tr50/util/json.h>
#include <tr50/util/mutex.h>

int tr50_mailbox_suspend(void *tr50) {
	if (tr50 == NULL) return ERR_TR50_PARMS;
	((_TR50_CLIENT *)tr50)->mailbox_suspend = TRUE;
	return 0;
}

int tr50_mailbox_resume(void *tr50, int check_immediately) {
	if (tr50 == NULL) return ERR_TR50_PARMS;
	((_TR50_CLIENT *)tr50)->mailbox_suspend = FALSE;
	if (check_immediately) {
		tr50_mailbox_check(tr50);
	}
	return 0;
}

void _tr50_mailbox_check_callback(void *tr50, int status, const void *request_message, void *message,void* custom) {
	JSON *methods, *params;
	int count = 0, i = 0;
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;

	if (status != 0) {
		log_debug("_tr50_mailbox_check_callback(): failed with status[%d]", status);
		goto check_for_another_mail_item;
	}

	params = tr50_reply_get_params(message,"1",FALSE);

	if ((methods = tr50_json_get_object_item(params, "messages")) == NULL) {
		log_debug("_tr50_mailbox_check_callback(): failed [messages param not found]");
		tr50_message_delete(message);
		goto check_for_another_mail_item;
	}

	count = tr50_json_get_array_size(methods);
	for (i = 0; i < count; ++i) {
		JSON *item = tr50_json_get_array_item(methods, i);
		const char *id = tr50_json_get_object_item_as_string(item, "id");
		const char *thing_key = tr50_json_get_object_item_as_string(item, "thingKey");
		const char *command = tr50_json_get_object_item_as_string(item, "command");
		const char *from = tr50_json_get_object_item_as_string(item, "from");
		JSON *params = tr50_json_get_object_item(item, "params");
		if (!id || !thing_key || !command) {
			continue;
		}
		if (strcmp(command, "method.exec") == 0) {
			const char* method = tr50_json_get_object_item_as_string(params, "method");
			JSON * method_params = tr50_json_get_object_item(params, "params");
			tr50_method_process(tr50, id, thing_key, method, from, method_params, custom);
		}
		tr50_command_process(tr50, id, thing_key, command, from, params,custom);
	}
	
	tr50_message_delete(message);
		
check_for_another_mail_item:
	_tr50_mutex_lock(client->mailbox_check_mux);
	client->mailbox_send_in_progress = FALSE;
	if ((count > 0) || (client->mailbox_another_request == TRUE)) {
		log_debug("_tr50_mailbox_check_callback(): another mailbox check being done.");
		client->mailbox_another_request = FALSE;
		_tr50_mutex_unlock(client->mailbox_check_mux);
		tr50_mailbox_check(tr50);
	}
	else  {
		_tr50_mutex_unlock(client->mailbox_check_mux);
	}
	
}

int tr50_mailbox_check(void *tr50) {
	int ret, id;
	void *request;
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;

	if (((_TR50_CLIENT *)tr50)->mailbox_suspend) return ERR_TR50_MAILBOX_SUSPENDED;

	//mutex lock - doing mail check
	_tr50_mutex_lock(client->mailbox_check_mux);
	if (client->mailbox_send_in_progress == TRUE) {
		client->mailbox_another_request = TRUE;
		_tr50_mutex_unlock(client->mailbox_check_mux);
		return ERR_TR50_MAILBOX_CHECK_IN_PROGRESS;
	} else {
		client->mailbox_send_in_progress = TRUE;
		_tr50_mutex_unlock(client->mailbox_check_mux);
	}
	
	_tr50_stats_mailbox_check_up(tr50);

	// send check
	if ((ret = tr50_message_create(&request)) != 0) {
		goto end_error;
	}

	tr50_message_add_command(request, "1", "mailbox.check", NULL);

	if ((ret = tr50_api_call_async(tr50, request, &id, _tr50_mailbox_check_callback, tr50, 30000)) != 0) {
		log_debug("_tr50_mailbox_check_callback(): tr50_api_call_async failed [%d]", ret);
		tr50_message_delete(request);
		goto end_error;
	}
	return 0;
end_error:
	_tr50_mutex_lock(client->mailbox_check_mux);
	client->mailbox_send_in_progress = FALSE;
	client->mailbox_another_request = FALSE;
	_tr50_mutex_unlock(client->mailbox_check_mux);
	return ret;
}
