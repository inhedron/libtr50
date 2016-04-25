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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <tr50/util/time.h>
#include <tr50/util/thread.h>
#include <tr50/util/blob.h>
#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/platform.h>
#include <tr50/tr50.h>

#if defined(_AT_SIMULATOR)
#include <he910/at.h>
#else
#include <tr50/wrappers/at.h>
#endif
 
#include <tr50/internal/tr50.h>

extern _AT_WRAPPER g_tr50_at_wrapper;

#if defined(_AT_SIMULATOR)

extern int ATDWRunPortAttach(int id);
extern int ATDWATRunPortDetach(int id);

int NVMRead(int family, int id, INT8 *buffer) {
	return -1;
}

void SYstrcpy(char *dest, char *source) {
	strcpy(dest, source);
}

#define DW_MAX_FEAT_OPTION_LENGTH	32
#define DWfeatEnMaskId		0
#define DWfeatOptions1Id	0
#define DWFeatEnDefault		0xFF
#else
#define DWFeatEnDefault		0
#endif

#define DW_FEAT_NUMBER_MAX	8
#define DW_FEAT_OPTION_MAX	5

int _tr50_at_status_get(int tr50_status) {
	// 0 = disconnected
	// 1 = trying to connect
	// 2 = connected
	// 3 = waiting to connect
	switch (tr50_status) {
	case TR50_STATUS_DISABLED:
	case TR50_STATUS_STOPPED:
	case TR50_STATUS_STOPPING:
		return 0;
	case TR50_STATUS_CONNECTED:
		return 2;
	case TR50_STATUS_RECONNECTING:
	case TR50_STATUS_CONNECTING:
		return 1;
		break;
	case TR50_STATUS_CREATED:
	case TR50_STATUS_BROKEN:
		return 3;
	}
	return 0;
}

int _tr50_at_feature_enabled(int id) {
	UINT8 features;

	if (NVMRead(DWCfg, DWfeatEnMaskId, &features)) {
		features = DWFeatEnDefault;
	}

	if (features & (0x01 << id)) {
		return 1;
	}
	return 0;
}

void _tr50_at_feature_option_get(int id, int option_id, char *buffer) {
	UINT8 allOptions[DW_MAX_FEAT_OPTION_LENGTH * 5];

	if (NVMRead(DWCfg, DWfeatOptions1Id + id, allOptions)) {
		_memory_memset(allOptions, 0x00, DW_MAX_FEAT_OPTION_LENGTH * 5);    //if error, set to default
	}

	_memory_memset(buffer, 0x00, DW_MAX_FEAT_OPTION_LENGTH + 1);
	_memory_memcpy(buffer, &allOptions[DW_MAX_FEAT_OPTION_LENGTH * option_id], DW_MAX_FEAT_OPTION_LENGTH);
}

int _at_tr50_should_reconnect(int disconnected_in_ms, int last_reconnect_in_ms, void *custom) {
	int opt = (int)custom;
	/*
			0 – auto - reconnect disabled
			1 – auto - reconnect lazy - on next send and every 3600 second.
			2 – auto - reconnect moderate(default) - every 120 seconds for an hour, then every 3600 second after the first day.
			3 – auto - reconnect aggressive - every 120 seconds.
	*/
	if (opt == 0) {
		return 0;
	} else if (opt == 1) {
		return last_reconnect_in_ms / 1000 > 3600;
	} else if (opt == 2) {
		if (disconnected_in_ms / 1000 > 86400) {
			return last_reconnect_in_ms / 1000 > 3600;
		}
		return last_reconnect_in_ms / 1000 > 120;
	} else if (opt == 3) {
		return last_reconnect_in_ms / 1000 > 120;
	}
	return last_reconnect_in_ms / 1000 > 120;
}

int _tr50_at_cmd_dwconn_set(void *token, void *output_blob) {
	int ret = ERR_TR50_PARMS;
	const char *state = at_token_parameter(token, 0);

	if (state[0] == '0') {
		ret = tr50_stop(g_tr50_at_wrapper.tr50);
		tr50_stats_clear_last_error(g_tr50_at_wrapper.tr50);

		if (_tr50_at_feature_enabled(0)) {
			ATDWATRunPortDetach(4);
		}
		return ret;
	} else if (state[0] == '1') {
		char *colon;
		INT8 url[DW_URL_LENGTH + 1] = "", appToken[DW_MAX_TOKEN_LENGTH + 1] = "";
		UINT8 devIdSelector, security, autoReconn, overflowHandle;
		UINT32 heartBeat;
		char buffer[32];
		int i, connect_error;
		// setup parameters

		if (NVMRead(DWCfg, DWUrlId, url)) {
			SYstrcpy(url, DW_URL_DEF);    //if error, set to default string
		}
		if (NVMRead(DWCfg, DWdevideIdSelId, &devIdSelector)) {
			devIdSelector = DW_DEV_ID_SEL_DEF;    //if error, set to default
		}
		if (NVMRead(DWCfg, DWappTokenId, appToken)) {
			SYstrcpy(appToken, DW_APP_TOKEN_DEF);    //if error, set to default string
		}
		if (NVMRead(DWCfg, DWsecurityId, &security)) {
			security = DW_SEC_DEF;    //if error, set to default
		}
		if (NVMRead(DWCfg, DWheartBeatId, (INT8 *)&heartBeat)) {
			heartBeat = DW_HEART_BEAT_DEF;    //if error, set to default
		}
		if (NVMRead(DWCfg, DWautoReconnId, &autoReconn)) {
			autoReconn = DW_AUTO_RECONN_DEF;    //if error, set to default
		}
		if (NVMRead(DWCfg, DWoverflowHandlId, &overflowHandle)) {
			overflowHandle = DW_OVERFL_HANDL_DEF;    //if error, set to default
		}
		log_important_info("_tr50_at_cmd_dwconn_set(): url[%s] dev[%d] app[%s] ssl[%d] heartbeat[%l] reconnect[%d] overflow[%d]",
						   url,
						   devIdSelector,
						   appToken,
						   security,
						   heartBeat,
						   autoReconn,
						   overflowHandle
						  );

		for (i = 0; i < DW_FEAT_NUMBER_MAX; ++i) {
			char option_buffer[64];
			int j;
			log_important_info("feature[%d]: is [%s]", i, _tr50_at_feature_enabled(i) ? "Enabled" : "Disabled");
			for (j = 0; j < DW_FEAT_OPTION_MAX; ++j) {
				_tr50_at_feature_option_get(i, j, option_buffer);
				log_important_info("feature[%d]: option[%d] is [%s]", i, j, option_buffer);
			}
		}

		if (_tr50_at_feature_enabled(0)) {
			ATDWRunPortAttach(4);
		}

		if (strlen(url) > 0) {
			if ((colon = strchr(url, ':')) != NULL) {
				*colon = 0;
				tr50_config_set_port(g_tr50_at_wrapper.tr50, atoi(colon + 1));
			}
			tr50_config_set_host(g_tr50_at_wrapper.tr50, url);
		}

		if (devIdSelector == 0) {
			_tr50_at_info_imei(buffer);
			tr50_config_set_username(g_tr50_at_wrapper.tr50, buffer);
			tr50_config_set_client_id(g_tr50_at_wrapper.tr50, buffer);
		} else {
			_tr50_at_info_ccid(buffer);
			tr50_config_set_username(g_tr50_at_wrapper.tr50, buffer);
			_tr50_at_info_imei(buffer);
			tr50_config_set_client_id(g_tr50_at_wrapper.tr50, buffer);
		}

		if (strlen(appToken) > 0) {
			tr50_config_set_password(g_tr50_at_wrapper.tr50, appToken);
		}

		tr50_config_set_keeplive(g_tr50_at_wrapper.tr50, heartBeat * 1000);

		tr50_config_set_should_reconnect_handler(g_tr50_at_wrapper.tr50, _at_tr50_should_reconnect, (void *)((int)autoReconn));

		if (security) {
			tr50_config_set_ssl(g_tr50_at_wrapper.tr50, 1);
			tr50_config_set_port(g_tr50_at_wrapper.tr50, 8883);
		}
		if ((ret = tr50_start2(g_tr50_at_wrapper.tr50, &connect_error)) != 0) {
			return ret;
		}
		return connect_error;
	}
	return ERR_TR50_PARMS;
}

int _tr50_at_cmd_dwconn_get(void *token, void *output_blob) {
	int status = tr50_current_status(g_tr50_at_wrapper.tr50);
	_blob_format_append(output_blob, "%d,%d", tr50_current_state(g_tr50_at_wrapper.tr50) == TR50_STATE_STARTED ? 1 : 0, _tr50_at_status_get(status));
	return 0;
}

void _tr50_at_send_reply(void* tr50, int status, const void *request_msg, void *reply, void *custom) {
	int msg_id, data_len, ret;
 
	void * message =NULL;
	const char* err_message = "Operation timed out.";
	JSON* err;
	log_debug("_tr50_at_send_reply ... status[%d]", status);

	if (status != 0) {
	
		//put storage (timeout)
		msg_id = 1;//tr50_message_get_seq_id((void *)request_msg);
		tr50_message_create(&message);
		((_TR50_MESSAGE *)message)->is_reply = TRUE;
		err = tr50_json_create_object();
		tr50_json_add_false_to_object(err, "success");
		tr50_json_add_item_to_object(err, "errorcodes", tr50_json_create_int_array(&status, 1));
		tr50_json_add_item_to_object(err, "errormessages", tr50_json_create_string_array(&err_message, 1));
		((_TR50_MESSAGE *)message)->json = err;
 
 		if ((ret = _tr50_at_storage_put(message, 0, msg_id, &data_len)) != 0) {
 
			log_important_info("_tr50_at_send_reply(): _tr50_at_storage_put failed [%d]", ret);
		}
		// notify
		if (g_tr50_at_wrapper.unsol_callback) {
			char *output = _memory_malloc(32 + 1);
			snprintf(output, 32, "%d,%d,%d", (int)custom, msg_id, data_len); //ring
			log_debug("_tr50_at_send_reply(): calling unsol with [%s]", output);
			g_tr50_at_wrapper.unsol_callback(output, strlen(output));
		}
		tr50_message_delete(message);
		return;
	}
 	msg_id = 1; // tr50_message_get_seq_id(reply);
 

	// put storage
	if ((ret = _tr50_at_storage_put(reply, (int)custom, msg_id, &data_len)) != 0) {
		log_important_info("_tr50_at_send_reply(): _tr50_at_storage_put failed [%d]", ret);
		return;
	}

	// notify
	if (g_tr50_at_wrapper.unsol_callback) {
		char *output = _memory_malloc(32 + 1);
		snprintf(output, 32, "%d,%d,%d", (int)custom, msg_id, data_len); //ring
		log_debug("_tr50_at_send_reply(): calling unsol with [%s]", output);
		g_tr50_at_wrapper.unsol_callback(output, strlen(output));
	}
}

int _tr50_at_cmd_dwsendr_set(void *token, void *output_blob) {
	int ret, id;
	void *message = NULL;
	char *at = at_token_data(token);
	char *len;
	char *data;

	if (_tr50_at_storage_is_full()) {
		ret = ERR_TR50_AT_STORAGE_FULL;
		_blob_format_append(output_blob, "Failed: %d", ret);
		return ret;
	}

	if ((len = strchr(at, '=')) == NULL || (data = strchr(at, ',')) == NULL) {
		_blob_format_append(output_blob, "Failed: No data specified");
		return ERR_AT_TOKEN_CMD_NOT_MATCH;
	}
	*data = 0;
	log_debug("_tr50_at_cmd_dwsendr_set(): length[%s] data[%s]", len + 1, data + 1);

	if ((ret = tr50_message_from_string(data + 1, atoi(len + 1), &message)) != 0) {
		if (message != NULL) {
			tr50_message_delete(message);
		}
		_blob_format_append(output_blob, "Failed: %d", ret);
		goto end_dwsendr;
	}

	if ((ret = tr50_api_call_async(g_tr50_at_wrapper.tr50, message, &id, _tr50_at_send_reply, (void *)TR50_AT_SEND_MODE_RAW, 30000)) != 0) {
		tr50_message_delete(message);
		_blob_format_append(output_blob, "Failed: %d", ret);
		goto end_dwsendr;
	}
	_blob_format_append(output_blob, "%d", id);
end_dwsendr:
	return ret;
}

void _tr50_parse_json_for_integers(JSON *params, int i, int count, void *token) {
	int is_number = 1, len, j;
	const char *value = at_token_parameter(token, i + 1);
	len = strlen(value);
	for (j = 0; j < len; ++j) {
#if defined(_AT_SIMULATOR)
		if (!isdigit(value[j]) && value[j] != '.') {
#else
		if (!SYisdigit(value[j]) && value[j] != '.') {
#endif
			is_number = 0;
			break;
		}
	}
	if (is_number) {
		if (strchr(value, '.')) {
			tr50_json_add_number_to_object(params, at_token_parameter(token, i), atof(value));
		} else {
			tr50_json_add_number_to_object(params, at_token_parameter(token, i), atoi(value));
		}
	} else if (len > 2 && value[0] == '"' && value[len - 1] == '"') {
		char *tmp_value = _memory_clone((char *)value + 1, len - 2);
		tr50_json_add_string_to_object(params, at_token_parameter(token, i), tmp_value);
		_memory_free(tmp_value);
	} else {
		tr50_json_add_string_to_object(params, at_token_parameter(token, i), value);
	}
}

int _tr50_at_cmd_dwsend_set_normal(void *token, void *output_blob) {
	const char *cmd = at_token_parameter(token, 1);
	int ret, i,  id, count = at_token_count(token);
 
	void *message =NULL;
 
	JSON *params;

	if (_tr50_at_storage_is_full()) {
		ret = ERR_TR50_AT_STORAGE_FULL;
		_blob_format_append(output_blob, "Failed: %d", ret);
		return ret;
	}

	if (count % 2) {
		ret = ERR_TR50_AT_UNEVEN_PARAMS;
		_blob_format_append(output_blob, "Failed: %d", ret);
		return ret;
	}

	params = tr50_json_create_object();

	for (i = 2; i + 1 < count; i += 2) {
		_tr50_parse_json_for_integers(params, i, count, token);
	}
 
	tr50_message_create(&message);
	tr50_message_add_command( message, "1", cmd, params);

	 
 

	if ((ret = tr50_api_call_async(g_tr50_at_wrapper.tr50, message, &id, _tr50_at_send_reply, (void *)TR50_AT_SEND_MODE_DELIMITED, 30000)) != 0) {
		tr50_message_delete(message);
		_blob_format_append(output_blob, "Failed: %d", ret);
		return ret;
	}
	_blob_format_append(output_blob, "%d", id);
	return ret;
}

int _tr50_at_cmd_dwsend_set_method(void *token, void *output_blob) {
	const char *thing_key = at_token_parameter(token, 1);
	const char *timeout = at_token_parameter(token, 2);
	const char *method = at_token_parameter(token, 3);
	const char *singleton = at_token_parameter(token, 4);
	int ret, i, id, count = at_token_count(token);
 
	void *message =NULL;
 
	JSON *params, *mparams;

	if (_tr50_at_storage_is_full()) {
		ret = ERR_TR50_AT_STORAGE_FULL;
		_blob_format_append(output_blob, "Failed: %d", ret);
		return ret;
	}

	if (count < 5 || count % 2 == 0) {
		ret = ERR_TR50_AT_UNEVEN_PARAMS;
		_blob_format_append(output_blob, "Failed: %d", ret);
		return ret;
	}

	params = tr50_json_create_object();
	tr50_json_add_string_to_object(params, "thingKey", thing_key);
	tr50_json_add_number_to_object(params, "ackTimeout", atoi(timeout));
	tr50_json_add_string_to_object(params, "method", method);
	tr50_json_add_bool_to_object(params, "singleton", atoi(singleton));

	mparams = tr50_json_create_object();
	for (i = 5; i < count; i += 2) {
		_tr50_parse_json_for_integers(mparams, i, count, token);
	}
	tr50_json_add_item_to_object(params, "params", mparams);
 
	tr50_message_create(&message);
	tr50_message_add_command(message, "1", "method.exec", params);
 
	if ((ret = tr50_api_call_async(g_tr50_at_wrapper.tr50, message, &id, _tr50_at_send_reply, (void *)TR50_AT_SEND_MODE_DELIMITED, atoi(timeout) + 5000)) != 0) {
		tr50_message_delete(message);
		_blob_format_append(output_blob, "Failed: %d", ret);
		return ret;
	}
	_blob_format_append(output_blob, "%d", id);
	return ret;
}

int _tr50_at_cmd_dwsend_set_method_update(void *token, void *output_blob) {
	const char *id = at_token_parameter(token, 1);
	const char *message = at_token_parameter(token, 2);
	int ret;

	if (id == NULL || message == NULL) {
		ret = ERR_TR50_PARMS;
		_blob_format_append(output_blob, "Failed: %d", ERR_TR50_PARMS);
		return ret;
	}

	if ((ret = tr50_command_update(g_tr50_at_wrapper.tr50, id, message)) == 0) {
		_blob_format_append(output_blob, "Failed: %d", ret);
		return ret;
	}
	_blob_format_append(output_blob, "%d", 0);
	return ret;
}

int _tr50_at_cmd_dwsend_set_method_ack(void *token, void *output_blob) {
	const char *id = at_token_parameter(token, 1);
	const char *status_str = at_token_parameter(token, 2);
	int ret, i, status;
	int count = at_token_count(token);
	JSON *params;

	if (id == NULL || status_str == NULL) {
		ret = ERR_TR50_PARMS;
		_blob_format_append(output_blob, "Failed: %d", ERR_TR50_PARMS);
		return ret;
	}

	if ((status = atoi(status_str)) != 0) {
		const char *message = at_token_parameter(token, 3);
		if ((ret = tr50_command_ack(g_tr50_at_wrapper.tr50, id, status, message, NULL)) == 0) {
			_blob_format_append(output_blob, "Failed: %d", ret);
			return ret;
		}
		return 0;
	}

	if (count < 3 || count % 2 == 0) {
		ret = ERR_TR50_AT_UNEVEN_PARAMS;
		_blob_format_append(output_blob, "Failed: %d", ret);
		return ret;
	}

	params = tr50_json_create_object();

	for (i = 3; i + 1 < count; i += 2) {
		_tr50_parse_json_for_integers(params, i, count, token);
	}

	if ((ret = tr50_command_ack(g_tr50_at_wrapper.tr50, id, 0, NULL, params)) == 0) {
		_blob_format_append(output_blob, "Failed: %d", ret);
		return ret;
	}
	_blob_format_append(output_blob, "%d", 0);
	return ret;
}

int _tr50_at_cmd_dwsend_set(void *token, void *output_blob) {
	const char *type = at_token_parameter(token, 0);
	if (type[0] == '0') {
		return _tr50_at_cmd_dwsend_set_normal(token, output_blob);
	} else if (type[0] == '1') {
		return _tr50_at_cmd_dwsend_set_method(token, output_blob);
	} else if (type[0] == '2') {
		return _tr50_at_cmd_dwsend_set_method_update(token, output_blob);
	} else if (type[0] == '3') {
		return _tr50_at_cmd_dwsend_set_method_ack(token, output_blob);
	}
	return ERR_AT_CMD_NOT_SUPPORTED;
}

int _tr50_at_cmd_dwlrcv_get(void *token, void *output_blob) {
	_tr50_at_storage_list(output_blob);
	return 0;
}

int _tr50_at_cmd_dwrcvr_set(void *token, void *output_blob) {
	int ret, output_len, status;
	const char *msg_id_str = at_token_parameter(token, 0);
	char *output;
	int msg_id = atoi(msg_id_str);

	if ((ret = _tr50_at_storage_get(msg_id, TR50_AT_SEND_MODE_RAW, &status, &output, &output_len)) != 0) {
		_blob_format_append(output_blob, "Failed : %d", ret);
		return 0;
	}

	_blob_format_append(output_blob, "%d,%d,%d,%s", msg_id, status, output_len, output);
	_memory_free(output);
	return 0;
}

int _tr50_at_cmd_dwrcv_set(void *token, void *output_blob) {
	int ret, output_len, status;
	const char *msg_id_str = at_token_parameter(token, 0);
	char *output;
	int msg_id = atoi(msg_id_str);

	if ((ret = _tr50_at_storage_get(msg_id, TR50_AT_SEND_MODE_DELIMITED, &status, &output, &output_len)) != 0) {
		_blob_format_append(output_blob, "Failed : %d", ret);
		return 0;
	}

	_blob_format_append(output_blob, "%d,%d,%d,%s", msg_id, status, output_len, output);
	if (output) {
		_memory_free(output);
	}
	return 0;
}

void _tr50_at_latency_callback(void* tr50, int status, const void *request_msg, void *message, void *custom) {
	long long *recv_timestamp = custom;
	if (status == 0) {
		if ((*recv_timestamp = tr50_reply_get_error_code(message, "1")) == 0) {
 
			*recv_timestamp = _time_now();
		}
		tr50_message_delete(message);
	} else {
		*recv_timestamp = status;
	}
}

int _tr50_at_latency() {
	int ret, id;
 
	void *message = NULL;
	long long recv_timestamp = 0, sent_time;
	tr50_message_create(&message);
	tr50_message_add_command( message, "1", "diag.ping", NULL);

	sent_time = _time_now();

	if ((ret = tr50_api_call_async(g_tr50_at_wrapper.tr50, message, &id, _tr50_at_latency_callback, &recv_timestamp, 5000)) != 0) {
		tr50_message_delete(message);
		return ret;
	}

	while (!recv_timestamp) {
		_thread_sleep(100);
	}
	if (recv_timestamp < 0) {
		ret = recv_timestamp;
	} else {
		ret = recv_timestamp - sent_time;
	}
	return ret;
}

int _tr50_at_cmd_dwstatus_get(void *token, void *output_blob) {
	int status = tr50_current_status(g_tr50_at_wrapper.tr50);

	//last error
	int last_error = tr50_stats_last_error(g_tr50_at_wrapper.tr50);

	//last latency
	int latency = -1;

	int byte_recv = tr50_stats_byte_recv(g_tr50_at_wrapper.tr50);
	int byte_sent = tr50_stats_byte_sent(g_tr50_at_wrapper.tr50);

	int pub_recv = tr50_stats_pub_recv(g_tr50_at_wrapper.tr50);
	int pub_sent = tr50_stats_pub_sent(g_tr50_at_wrapper.tr50);

	if (status == TR50_STATUS_CONNECTED) {
		latency = _tr50_at_latency();
		last_error = 0;
	}

	_blob_format_append(output_blob, "%d,%d,%d,%d,%d,%d,%d", _tr50_at_status_get(status), last_error, latency, pub_recv, pub_sent, byte_recv, byte_sent);
	return 0;
}
