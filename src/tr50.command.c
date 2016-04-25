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

 

int tr50_command_register(void *tr50, const char *cmd, tr50_command_callback func) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	_TR50_COMMAND *command = (_TR50_COMMAND *)_memory_malloc(sizeof(_TR50_COMMAND));
	_memory_memset(command, 0, sizeof(_TR50_COMMAND));

	command->callback = func;
	strncpy(command->name, cmd, TR50_COMMAND_NAME_LEN);
	command->next = client->command_head;
	client->command_head = command;
	log_important_info("tr50_command_register(): cmd[%s] registered.", command->name);
	return 0;
}
 
int tr50_command_process(void *tr50, const char *id, const char *key, const char *command, const char *from, JSON *params, void * custom) {
	_TR50_CLIENT *client = (_TR50_CLIENT *)tr50;
	int ret;
	_TR50_COMMAND *cmd = client->command_head;

	if (!command) {
		return ERR_TR50_CMD_UNKNOWN;
	}

	if (strcmp(command, "method.exec") == 0 && (tr50_is_method_registered()== TRUE)) {
		return 0;
	}
	while(cmd) {
		if(strcmp(cmd->name,command)==0) {
			long long ended, started;
			log_debug("tr50_command_process(): executing cmd[%s] ...", cmd->name);
			started = _time_now();
			if((ret=cmd->callback(id,key,command,from,params))!=0) {
				log_important_info("tr50_command_process(): cmd[%s] failed [%d]", command, ret);
			}
			ended = _time_now();
			if (ended - started > 1000) {
				log_need_investigation("tr50_command_process(): cmd callback for [%s] is taking [%d]ms.", cmd->name, (int)(ended - started));
			}
			return ret;
		}
		cmd = (_TR50_COMMAND *)cmd->next;
	}
	log_need_investigation("tr50_command_process(): unknown cmd[%s] recv'ed", command);
	tr50_command_ack(tr50, id, ERR_TR50_CMD_UNKNOWN, "Unknown command", NULL);
	return ERR_TR50_CMD_UNKNOWN;
}

int tr50_command_update(void *tr50, const char *id, const char *msg) {
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

int tr50_command_ack(void *tr50, const char *id, int status, const char *err_message, JSON *ack_params) {
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
