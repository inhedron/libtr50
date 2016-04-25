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

#if defined (_AT_SIMULATOR)
int ATDWRunPortAttach(int id) {
	return 1;
}
int ATDWATRunPortDetach(int id) {
	return 1;
}
#else
#include "sy/portab.h"
#include <dwmain.h>
#endif

#include <string.h>

#if defined(_AT_SIMULATOR)
#include <he910/at.h>
#else
#include <tr50/wrappers/at.h>
#endif

#include <tr50/tr50.h>
#include <tr50/util/log.h>
#include <tr50/util/thread.h>
#include <tr50/util/memory.h>
#include <tr50/util/blob.h>
#include <tr50/util/mutex.h>
#include <tr50/util/time.h>

#define TR50_AT_EXEC_TTL 60000

extern _AT_WRAPPER g_tr50_at_wrapper;
char *g_tr50_at_wrapper_mail_id = NULL;
long long g_tr50_at_wrapper_mail_timestamp = 0;

int _tr50_at_run_exec(const char *input) {
	int ret = 0;
#if !defined (_AT_SIMULATOR)
	int input_len, remaining;

	input_len = remaining = strlen(input);;

	log_debug("_at_process_send(): input[%s]", input);
	while (remaining > 0) {
		DW_TCP_RUN_AT_DATA_TYPE *pMsg = (DW_TCP_RUN_AT_DATA_TYPE *)SYMalloc(sizeof(DW_TCP_RUN_AT_DATA_TYPE));
		pMsg->len = remaining;
		pMsg->pData = (UINT8 *)SYMalloc(remaining + 2);
		SYstrcpy(pMsg->pData, input + input_len - remaining);
		pMsg->pData[remaining] = 0x0D;
		pMsg->pData[remaining + 1] = 0x00;
		++pMsg->len;
		ret = ExecuteTCPRunATCmd(pMsg); // ret is the bytes sent, if this does not equal to the len, part of the message needs to be resend.
		log_debug("_tr50_at_run_exec(%d): returns [%d]", remaining, ret);
		remaining -= ret;
	}
#else
	log_debug("_at_process_send(): input[%s]", input);
	_blob_format_append(g_tr50_at_wrapper.at_run_buffer, "AT...OK");
#endif
	log_debug("_at_process_send(): ExecuteTCPRunATCmd returns [%d]", ret);
	return 0;
}

int _tr50_at_process_run_exec(const char *id, const char *thing_key, const char *command, const char *from, JSON *params) {
	int *timeout = tr50_json_get_object_item_as_int(params, "timeout");
	const char *input = tr50_json_get_object_item_as_string(params, "input");

	if (!timeout || !input) {
		return ERR_TR50_PARMS;
	}

	_tr50_mutex_lock(g_tr50_at_wrapper.at_run_mux);
	if (g_tr50_at_wrapper_mail_id) {
		if (_time_now() - g_tr50_at_wrapper_mail_timestamp < TR50_AT_EXEC_TTL) {
			tr50_command_ack(g_tr50_at_wrapper.tr50, id, ERR_TR50_AT_EXEC_IN_PROGRESS, "System is still processing the previous command", NULL);
			_tr50_mutex_unlock(g_tr50_at_wrapper.at_run_mux);
			return 0;
		}
		log_important_info("_tr50_at_process_run_exec(): previous module.exec has timeout.");
		_memory_free(g_tr50_at_wrapper_mail_id);
	}

	g_tr50_at_wrapper_mail_id = _memory_clone((char*)id, strlen(id));
	g_tr50_at_wrapper_mail_timestamp = _time_now();

	log_debug("_tr50_at_process_run_exec(): command id [%s]", g_tr50_at_wrapper_mail_id);

	if (!ATDWRunPortAttach(4)) {
		tr50_command_ack(g_tr50_at_wrapper.tr50, id, ERR_TR50_AT_ATTACH_FAIL, "ATDWRunPortAttach failed", NULL);
		_memory_free(g_tr50_at_wrapper_mail_id);
		g_tr50_at_wrapper_mail_id = NULL;
		_tr50_mutex_unlock(g_tr50_at_wrapper.at_run_mux);
		return 0;
	}

	_blob_clear(g_tr50_at_wrapper.at_run_buffer);

	if (_tr50_at_run_exec(input) != 0) {
		tr50_command_ack(g_tr50_at_wrapper.tr50, id, ERR_TR50_AT_EXEC_FAIL, "ExecuteTCPRunATCmd failed", NULL);
		_memory_free(g_tr50_at_wrapper_mail_id);
		g_tr50_at_wrapper_mail_id = NULL;
		_tr50_mutex_unlock(g_tr50_at_wrapper.at_run_mux);
		return 0;
	}
	_tr50_mutex_unlock(g_tr50_at_wrapper.at_run_mux);
	return 0;
}

void tr50_at_run_send_back(void) {
	JSON *reply_params;

	_tr50_mutex_lock(g_tr50_at_wrapper.at_run_mux);

	if (!g_tr50_at_wrapper_mail_id) {
		log_important_info("tr50_at_run_send_back(): No pending module.exec request");
		_tr50_mutex_unlock(g_tr50_at_wrapper.at_run_mux);
		return;
	}
	log_debug("tr50_at_run_send_back(): command id [%s]", g_tr50_at_wrapper_mail_id);

	reply_params = tr50_json_create_object();
	tr50_json_add_string_to_object(reply_params, "output", (const char *)_blob_get_buffer(g_tr50_at_wrapper.at_run_buffer));
	tr50_command_ack(g_tr50_at_wrapper.tr50, g_tr50_at_wrapper_mail_id, 0, NULL, reply_params);

	_memory_free(g_tr50_at_wrapper_mail_id);
	g_tr50_at_wrapper_mail_id = NULL;
	_tr50_mutex_unlock(g_tr50_at_wrapper.at_run_mux);
}

int tr50_at_run_recv(const char *output, int output_len) {
	if (output_len < 1024) {
		log_debug("_at_process_recv(): [%s]", output);
	} else {
		log_debug("_at_process_recv(): output too long to print.");
	}
	_blob_append(g_tr50_at_wrapper.at_run_buffer, output, output_len);
	return output_len;
}

int _tr50_at_service_diag(const char *id, const char *thing_key, const char *command, const char *from, JSON *params) {
	JSON *reply_params = tr50_json_create_object();
	tr50_json_add_number_to_object(reply_params, "memory.handle", _memory_handle_count());
	tr50_json_add_number_to_object(reply_params, "tr50.byte_recv", tr50_stats_byte_recv(g_tr50_at_wrapper.tr50));
	tr50_json_add_number_to_object(reply_params, "tr50.byte_sent", tr50_stats_byte_sent(g_tr50_at_wrapper.tr50));
	tr50_json_add_number_to_object(reply_params, "tr50.reconnect_attempt_count", tr50_stats_reconnect_attempt_count(g_tr50_at_wrapper.tr50));
	tr50_json_add_number_to_object(reply_params, "tr50.reconnect_count", tr50_stats_reconnect_count(g_tr50_at_wrapper.tr50));
	tr50_command_ack(g_tr50_at_wrapper.tr50, id, 0, NULL, reply_params);
	return 0;
}

int _tr50_at_diag_ping(const char *id, const char *thing_key, const char *command, const char *from, JSON *params) {
	tr50_command_ack(g_tr50_at_wrapper.tr50, id, 0, NULL, NULL);
	return 0;
}

int at_process_register_all() {
	tr50_command_register(g_tr50_at_wrapper.tr50, "module.exec", _tr50_at_process_run_exec);
	tr50_command_register(g_tr50_at_wrapper.tr50, "module.diag", _tr50_at_service_diag);
	tr50_command_register(g_tr50_at_wrapper.tr50, "diag.ping", _tr50_at_diag_ping);
	return 0;
}
