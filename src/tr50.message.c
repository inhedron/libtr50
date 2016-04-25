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
#include <tr50/util/json.h>
#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/tr50.h>
#include <tr50/internal/tr50.h>

int tr50_message_create(void **message) {
	_TR50_MESSAGE *msg;
	if ((msg = (_TR50_MESSAGE *)_memory_malloc(sizeof(_TR50_MESSAGE))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(msg, 0, sizeof(_TR50_MESSAGE));

	msg->json = tr50_json_create_object();

	*message = msg;
	return 0;
}

int tr50_message_add_command(void *message, const char *cmd_id, const char *cmd, JSON *params) {
	_TR50_MESSAGE *msg = (_TR50_MESSAGE *)message;
	JSON *json;

	if(message==NULL || cmd_id==NULL || cmd==NULL) return ERR_TR50_PARMS;

	json = tr50_json_create_object();
	tr50_json_add_string_to_object(json,"command",cmd);
	if(params!=NULL) {
		tr50_json_add_item_to_object(json,"params",params);
	}
	tr50_json_add_item_to_object(msg->json,cmd_id,json);

	return 0;
}

int tr50_reply_get_error_code(void *reply, const char *cmd_id) {
	_TR50_MESSAGE *msg = (_TR50_MESSAGE *)reply;
	int *success;

	if(reply==NULL || cmd_id==NULL) return ERR_TR50_PARMS;
	
	if ((success = tr50_json_get_object_item_as_bool(msg->json, "success")) != NULL) {
		// check for global error
		if(*success) {
			// you will never get a "root" level success:true
			return ERR_TR50_REPLY_INVALID;
		} else {
			JSON *error_codes = tr50_json_get_object_item(msg->json, "errorcodes");
			if(error_codes) {
				int *ec = tr50_json_get_object_item_as_int(error_codes, 0);
				if(ec) {
					return *ec;
				} else {
					return ERR_TR50_CODE_MISSING;
				}
			} else {
				return ERR_TR50_CODE_MISSING;
			}
		}
	} else {
		// check for command error
		JSON *cmd = tr50_json_get_object_item(msg->json,cmd_id);
		if(cmd) {
			if ((success = tr50_json_get_object_item_as_bool(cmd, "success")) != NULL) {
				// check for global error
				if(*success) {
					return 0;
				} else {
					JSON *error_codes = tr50_json_get_object_item(cmd, "errorcodes");
					if(error_codes) {
						int *ec = tr50_json_get_object_item_as_int(error_codes, 0);
						if(ec) {
							return *ec;
						} else {
							return ERR_TR50_CODE_MISSING;
						}
					} else {
						return ERR_TR50_CODE_MISSING;
					}
				}
			}
		} else {
			return ERR_TR50_CMD_ID_NOT_FOUND;
		}
	}
	return 0;
}

const char *tr50_reply_get_error_message(void *reply, const char *cmd_id) {
	_TR50_MESSAGE *msg = (_TR50_MESSAGE *)reply;
	int *success;

	if(reply==NULL || cmd_id==NULL) return NULL;
	
	if ((success = tr50_json_get_object_item_as_bool(msg->json, "success")) != NULL) {
		// check for global error
		if(*success) {
			return NULL;
		} else {
			JSON *error_messages = tr50_json_get_object_item(msg->json, "errormessages");
			if(error_messages) {
				return tr50_json_get_object_item_as_string(error_messages, 0);
			} else {
				return NULL;
			}
		}
	} else {
		// check for command error
		JSON *cmd = tr50_json_get_object_item(msg->json,cmd_id);
		if(cmd) {
			if ((success = tr50_json_get_object_item_as_bool(cmd, "success")) != NULL) {
				// check for global error
				if(*success) {
					return 0;
				} else {
					JSON *error_messages = tr50_json_get_object_item(cmd, "errormessages");
					if(error_messages) {
						return tr50_json_get_object_item_as_string(error_messages, 0);
					} else {
						return NULL;
					}
				}
			}
		} else {
			return NULL;
		}
	}
	return 0;
}

int tr50_reply_get_error(void *reply, const char *cmd_id, char **error_message) {
	_TR50_MESSAGE *msg = (_TR50_MESSAGE *)reply;
	int *success;
	int ret=0;

	if(reply==NULL || cmd_id==NULL) return ERR_TR50_PARMS;
	
	if ((success = tr50_json_get_object_item_as_bool(msg->json, "success")) != NULL) {
		JSON *error_codes;

		// check for global error
		if(*success) {
			return 0;
		}

		if( (error_codes = tr50_json_get_object_item(msg->json, "errorcodes")) != NULL ) {
			int *ec = tr50_json_get_object_item_as_int(error_codes, 0);
			if(ec) ret=*ec;
		}

		if(error_message!=NULL) {
			JSON *error_messages = tr50_json_get_object_item(msg->json, "errormessages");
			if(error_messages) {
				const char *em = tr50_json_get_object_item_as_string(error_messages, 0);
				*error_message=_memory_clone((void*)em,strlen(em)+1);
			}
		}
	} else {
		// check for command error
		JSON *cmd = tr50_json_get_object_item(msg->json,cmd_id);
		if(cmd) {
			if ((success = tr50_json_get_object_item_as_bool(cmd, "success")) != NULL) {
				JSON *error_codes;

				// check for global error
				if(*success) {
					return 0;
				}

				if( (error_codes = tr50_json_get_object_item(cmd, "errorcodes")) != NULL ) {
					int *ec = tr50_json_get_object_item_as_int(error_codes, 0);
					if(ec) ret=*ec;
				}
				if(error_message!=NULL) {
					JSON *error_messages = tr50_json_get_object_item(cmd, "errormessages");
					if(error_messages) {
						const char *em = tr50_json_get_object_item_as_string(error_messages, 0);
						*error_message=_memory_clone((void*)em,strlen(em)+1);
					}
				}
			}
		} else {
			return ERR_TR50_CMD_ID_NOT_FOUND;
		}
	}
	return ret;
}

JSON *tr50_reply_get_params(void *reply, const char *cmd_id, int detach) {
	_TR50_MESSAGE *msg = (_TR50_MESSAGE *)reply;
	JSON *cmd;

	if(reply==NULL || cmd_id==NULL) return NULL;
	
	if( (cmd = tr50_json_get_object_item(msg->json,cmd_id)) != NULL ) {
		if(detach) {
			return tr50_json_detach_item_from_object(cmd, "params");
		} else {
			return tr50_json_get_object_item(cmd, "params");
		}
	}
	return NULL;
}

int tr50_message_delete(void *message) {
	_TR50_MESSAGE *msg = (_TR50_MESSAGE *)message;

	if(message==NULL) return ERR_TR50_PARMS;

	if (msg->json) {
		tr50_json_delete(msg->json);
	}

	_memory_free(message);
	return 0;
}

int tr50_reply_delete(void *message) {
	return tr50_message_delete(message);
}

void tr50_message_printf(const char *who, void *message) {
	log_debug("###### %s ######\n", who);
	//log_debug("tr50_message_get_cmd(message): [%s]\n", tr50_message_get_cmd(message));
	//log_debug("tr50_message_get_id(message): [%d]\n", tr50_message_get_id(message));
	//log_debug("tr50_message_get_params(message): [%s]\n", tr50_json_print(tr50_message_get_params(message)));
}

