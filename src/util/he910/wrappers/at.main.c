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

#if defined(_AT_SIMULATOR)
#include <he910/at.h>
#else
#include <tr50/wrappers/at.h>
#endif

#include <tr50/util/blob.h>
#include <tr50/util/platform.h>
#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/mutex.h>
#include <tr50/tr50.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

_AT_WRAPPER g_tr50_at_wrapper;

int at_process_register_all();
void _tr50_at_state_change_handler(int previous_state, int current_state, const char *why, void *custom);

void *tr50_at_handle() {
	return g_tr50_at_wrapper.tr50;
}

// method.exec
int _tr50_method_event_execute(const char *id, const char *thing_key, const char *commmand, const char *from, JSON *params) {
	int ret, len;
	void *blob;
	const char *thingdef_key;
	const char *method;
	JSON *ptr, *method_params;
	int msg_id;

	thingdef_key = tr50_json_get_object_item_as_string(params, "thingDefKey");
	method = tr50_json_get_object_item_as_string(params, "method");
	method_params = tr50_json_get_object_item(params, "params");
	msg_id = tr50_api_msg_id_next(g_tr50_at_wrapper.tr50);

	if (!thingdef_key || !method || !method_params) {
		log_debug("_tr50_method_event_execute(): ...null");
		return ERR_TR50_PARMS;
	}

	log_debug("_tr50_method_event_execute(): id[%s] key[%s] method[%s]", id, thing_key, method);
	_blob_create(&blob, 512);
	_blob_format_append(blob, "%s,%s,%s", id, thingdef_key, method);
	ptr = method_params->child;
	while (ptr) {
		switch (ptr->type) {
		case JSON_FALSE:
			_blob_format_append(blob, ",%s,false", ptr->string);
			break;
		case JSON_TRUE:
			_blob_format_append(blob, ",%s,true", ptr->string);
			break;
		case JSON_NUMBER: {
			int i = ptr->valuedouble;
			if (ptr->valuedouble == ((double)i)) {
				_blob_format_append(blob, ",%s,%d", ptr->string, ptr->valueint);
			} else {
				_blob_format_append(blob, ",%s,%lf", ptr->string, ptr->valuedouble);
			}
			break;
		}
		case JSON_STRING:
			_blob_format_append(blob, ",%s,%s", ptr->string, ptr->valuestring);
			break;
		default:
			break;
		}
		ptr = ptr->next;
	}
	if ((ret = _tr50_at_storage_put(_blob_get_buffer(blob), TR50_AT_SEND_MODE_METHOD, msg_id, &len)) != 0) {
		tr50_command_ack(g_tr50_at_wrapper.tr50, id, ret, NULL, NULL);
		_blob_delete(blob);
		return ret;
	}
	_blob_delete_object(blob);

	// notify ring
	if (g_tr50_at_wrapper.unsol_callback) {
		char *output = _memory_malloc(128 + 1);
		snprintf(output, 128, "%d,%d,%d", TR50_AT_SEND_MODE_METHOD, msg_id, len);
		g_tr50_at_wrapper.unsol_callback(output, strlen(output));
		log_debug("_tr50_method_event_execute(): calling unsol with [%s]", output);
	}
	return 0;
}

int tr50_at_init(tr50_at_unsol_callback ring_callback) {
	_memory_memset(&g_tr50_at_wrapper, 0, sizeof(_AT_WRAPPER));

	log_filter_maximum_log_level(LOG_TYPE_LOW_LEVEL);
	tr50_create(&g_tr50_at_wrapper.tr50, "AT Wrapper", "demo.deviceiwse.com", 1883);
	tr50_config_set_state_change_handler(g_tr50_at_wrapper.tr50, _tr50_at_state_change_handler, NULL);

	_tr50_mutex_create(&g_tr50_at_wrapper.at_run_mux);
	_blob_create(&g_tr50_at_wrapper.at_run_buffer, 512);
	g_tr50_at_wrapper.unsol_callback = ring_callback;
	at_process_register_all();
	tr50_command_register(g_tr50_at_wrapper.tr50, "method.exec", _tr50_method_event_execute);
	return 0;
}

void _tr50_at_module_register() {
	if (!g_tr50_at_wrapper.at_connected_once) {
		JSON *params;
 
		void *request =NULL;
 
		char modem_buffer[32], version_buffer[64];

		g_tr50_at_wrapper.at_connected_once = 1;

		params = tr50_json_create_object();
		tr50_json_add_string_to_object(params, "make", "Telit");
		tr50_json_add_string_to_object(params, "model", _tr50_at_info_model(modem_buffer));
		tr50_json_add_string_to_object(params, "fwVersion", _tr50_at_info_version(version_buffer));

		if (_tr50_at_feature_enabled(0)) {
			tr50_json_add_true_to_object(params, "remAt");
		} else {
			tr50_json_add_false_to_object(params, "remAt");
		}
 
		tr50_message_create(&request);
		tr50_message_add_command( request,"1", "module.register", params);

 		if (tr50_api_call(g_tr50_at_wrapper.tr50, request) != 0) {
 
			tr50_message_delete(request);
		}
	}
}

void _tr50_at_state_change_handler(int previous_state, int current_state, const char *why, void *custom) {
	if (previous_state == TR50_STATUS_CONNECTING && current_state == TR50_STATUS_CONNECTED) {
		_tr50_at_module_register();
	} else if (previous_state == TR50_STATUS_RECONNECTING && current_state == TR50_STATUS_CONNECTED) {
		_tr50_at_module_register();
	}
}

// process an incoming AT message
int tr50_at_process(const char *at_input, char **at_output, int *at_output_len, char **data, int *data_len) {
	int ret = ERR_AT_CMD_NOT_SUPPORTED;
	void *token;
	void *result_blob;
	int is_data = 0;
	*data = NULL;
	*at_output = NULL;

	_blob_create(&result_blob, 256);
	at_token_create(&token, at_input);

	log_debug("tr50_at_process ... input[%s]", at_input);
	if (at_token_continue_with(token, "AT#DWCONN")) {
		if (at_token_continue_with(token, "?")) {
			ret = _tr50_at_cmd_dwconn_get(token, result_blob);
		} else if (at_token_continue_with(token, "=")) {
			ret = _tr50_at_cmd_dwconn_set(token, result_blob);
		}
	} else if (at_token_continue_with(token, "AT#DWSENDR")) {
		if (at_token_continue_with(token, "=")) {
			ret = _tr50_at_cmd_dwsendr_set(token, result_blob);
		}
	} else if (at_token_continue_with(token, "AT#DWSEND")) {
		if (at_token_continue_with(token, "=")) {
			ret = _tr50_at_cmd_dwsend_set(token, result_blob);
		}
	} else if (at_token_continue_with(token, "AT#DWLRCV")) {
		if (at_token_has_ended(token)) {
			ret = _tr50_at_cmd_dwlrcv_get(token, result_blob);
		}
	} else if (at_token_continue_with(token, "AT#DWRCVR")) {
		if (at_token_continue_with(token, "=")) {
			ret = _tr50_at_cmd_dwrcvr_set(token, result_blob);
			is_data = 1;
		}
	} else if (at_token_continue_with(token, "AT#DWRCV")) {
		if (at_token_continue_with(token, "=")) {
			ret = _tr50_at_cmd_dwrcv_set(token, result_blob);
			is_data = 1;
		}
	} else if (at_token_continue_with(token, "AT#DWSTATUS")) {
		if (at_token_has_ended(token)) {
			ret = _tr50_at_cmd_dwstatus_get(token, result_blob);
		}
	}

	log_debug("tr50_at_process ... done with command");
	at_token_delete(token);
	if (is_data) {
		if ((*data_len = _blob_get_length(result_blob)) > 0) {
			*data = (char *)_blob_get_buffer(result_blob);
			_blob_delete_object(result_blob);
		} else {
			ret = -1; // no data which is bad
			_blob_delete(result_blob);
		}
		log_debug("tr50_at_process ... data len[%d]", *data_len);
	} else {
		if ((*at_output_len = _blob_get_length(result_blob)) > 0) {
			*at_output = (char *)_blob_get_buffer(result_blob);
			_blob_delete_object(result_blob);
		} else {
			_blob_delete(result_blob);
		}
		log_debug("tr50_at_process ... at output len[%d]", *at_output_len);
	}
	log_debug("tr50_at_process ... returning [%d]", ret);
	return ret;
}
