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
#include <tr50/util/blob.h>
#include <tr50/worker.h>
#include <tr50/tr50.h>
#include <tr50/util/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <tr50/util/tcp.h>
#include <tr50/util/platform.h>
#if defined (_WIN32)
#  define strtok_r strtok_s
#  include <ctype.h>
#endif
 
#define TR50_MAILBOX_SEND_TIMEOUT_BUFFER	 5000
#define TR50_METHOD_TIMEOUT_BUFFER			 5000
#define TR50_DEFAULT_TIMEOUT				 5000
#define READ_SIZE 1024
#define POST_COMMAND "POST /file/%s HTTP/1.1\r\nHost:%s:80\r\nConnection: close\r\nTransfer-Encoding: chunked\r\nContent-Type: application/octet-stream\r\n\r\n"
#define GET_COMMAND "GET /file/%s HTTP/1.1\r\nHost:%s:80\r\nConnection: close\r\n\r\n" 
int send_json(void *tr50, const char *cmd, JSON *params, JSON **reply_params, char **error_msg) {
	void *message;
	int ret;
	int timeout = 0;
	int id;

	if ((ret = tr50_message_create(&message)) != 0) {
		tr50_json_delete(params);
		return ret;
	}

	if ((ret = tr50_message_add_command(message,"1", cmd, params)) != 0) {
		tr50_json_delete(params);
		tr50_message_delete(message);
		return ret;
	}

	if (tr50_json_get_object_item_as_int(params, "timeout") == NULL) {
		timeout = TR50_DEFAULT_TIMEOUT;
	} else {
		timeout = *tr50_json_get_object_item_as_int(params, "timeout");
		tr50_json_delete_item_from_object(params, "timeout");
	}

	if ((ret = tr50_api_call_async(tr50, message, &id, NULL, NULL, timeout)) != 0) {
		tr50_message_delete(message);
		return ret;
	}
	return 0;
}

int send_json_sync(void *tr50, const char *cmd, JSON *params, JSON **reply_params, char **error_msg) {
	void *message, *reply = NULL;
	int ret;
	int timeout = 0;

	if ((ret = tr50_message_create(&message)) != 0) {
		tr50_json_delete(params);
		return ret;
	}

	if ((ret = tr50_message_add_command(message, "1", cmd, params)) != 0) {
		tr50_json_delete(params);
		tr50_message_delete(message);
		return ret;
	}

	if (tr50_json_get_object_item_as_int(params, "timeout") == NULL) {
		timeout = TR50_DEFAULT_TIMEOUT;
	} else {
		timeout = *tr50_json_get_object_item_as_int(params, "timeout");
		tr50_json_delete_item_from_object(params, "timeout");
	}

	if ((ret = tr50_api_call_sync(tr50, message, &reply, timeout)) != 0) {
		tr50_message_delete(message);
		return ret;
	}

	if ((ret = tr50_reply_get_error_code(reply,"1")) != 0) {
		const char *error_message = tr50_reply_get_error_message(reply, "1");
		if ((error_message != NULL) && (error_msg != NULL)) {
			*error_msg = (char *)_memory_clone((void *)error_message, strlen(error_message));
		}
		tr50_reply_delete(reply);
		return ret;
	} else if (reply_params != NULL) {
		*reply_params = (JSON *)tr50_reply_get_params(reply, "1", TRUE);
	}

	if (reply != NULL) {
		tr50_reply_delete(reply);
	}
	return 0;
}

int send_json_async(void *tr50, const char *cmd, JSON *params, int *id, tr50_async_reply_callback callback, void *custom_cb_obj, int timeout) {
	void *message;
	int ret;

	if ((ret = tr50_message_create(&message)) != 0) {
		tr50_json_delete(params);
		return ret;
	}
	if ((ret = tr50_message_add_command(message, "1", cmd, params)) != 0) {
		tr50_json_delete(params);
		tr50_message_delete(message);
		return ret;
	}

	if ((ret = tr50_api_call_async(tr50, message, id, callback, custom_cb_obj, timeout)) != 0) {
		tr50_message_delete(message);
		return ret;
	} 
	return ret;
}

TR50_EXPORT int tr50_method_exec_ex_async(void *tr50, const char *method, int *id, JSON *req_params, void *optional_params, tr50_async_reply_callback callback, void *custom_cb_obj, int timeout) {
	int ret;
	JSON *params = NULL;

	if (method == NULL) {
		if (req_params) tr50_json_delete(req_params);
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			if (req_params) tr50_json_delete(req_params);
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "method", method);
	tr50_json_add_item_to_object(params, "params", req_params);

	if ((ret = send_json_async(tr50, "method.exec", params, id, callback, custom_cb_obj, timeout)) != 0) {
 		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_method_exec_ex_sync(void *tr50, const char *method, JSON *req_params, void *optional_params, void **optional_reply) {
	int ret;
	JSON *params = NULL;
	JSON *reply = NULL;

	if (method == NULL) {
		if (req_params) tr50_json_delete(req_params);
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			if (req_params) tr50_json_delete(req_params);
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "method", method);
	tr50_json_add_item_to_object(params, "params", req_params);

	if ((ret = send_json(tr50, "method.exec", params, &reply, NULL)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_alarm_publish_ex(void *tr50, const char *alarm_key, const int state, void *optional_params, void **optional_reply, char **error_msg) {
	int ret;
	JSON *params = NULL;
	JSON *reply = NULL;

	if (alarm_key == NULL ) {
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "key", alarm_key);
	tr50_json_add_number_to_object(params, "state", state);

	if ((ret = send_json_sync(tr50, "alarm.publish", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_location_publish_ex(void *tr50, const double lat_value, const double long_value, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	/* Latitude */
	tr50_json_add_number_to_object(params, "lat", lat_value);

	/* Longitude */
	tr50_json_add_number_to_object(params, "lng", long_value);

	if ((ret = send_json_sync(tr50, "location.publish", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}


TR50_EXPORT int tr50_log_publish_ex(void *tr50, const char *msg, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;

	if (msg == NULL) {
 		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "msg", msg);

	if ((ret = send_json_sync(tr50, "log.publish", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_mailbox_send_ex(void *tr50, const char *mailbox_cmd, JSON *req_params, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;

 	if (mailbox_cmd == NULL) {
		if (req_params) tr50_json_delete(req_params);
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			if (req_params) tr50_json_delete(req_params);
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "command", mailbox_cmd);
	tr50_json_add_item_to_object(params, "params", req_params);

	if ((ret = send_json_sync(tr50, "mailbox.send", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}



TR50_EXPORT int tr50_property_publish_ex(void *tr50, const char *prop_key, const double value, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;

	if (prop_key == NULL) {
		if (optional_params) tr50_json_delete((JSON*)optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "key", prop_key);
	tr50_json_add_number_to_object(params, "value", value);

	if ((ret = send_json_sync(tr50, "property.publish", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_property_current_ex(void *tr50, const char *prop_key, double *value, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;

	if (prop_key == NULL) {
 		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "key", prop_key);

	if ((ret = send_json_sync(tr50, "property.current", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	} else {
		*value = *((double*)tr50_json_get_object_item_as_double(reply, "value"));
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}

		return 0;
	}
}

TR50_EXPORT int tr50_thing_attr_set_ex(void *tr50, const char *key, const char *value, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;

	if ((key == NULL) || (value == NULL)) {
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "key", key);
	tr50_json_add_string_to_object(params, "value", value);

	if ((ret = send_json_sync(tr50, "thing.attr.set", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_thing_attr_get_ex(void *tr50, const char *key, char **value, void *optional_params, void **optional_reply, char **error_msg) {
	int ret;
	JSON *params = NULL;
	JSON *reply = NULL;

	if (key == NULL ) {
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = (JSON *) optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "key", key);

	if ((ret = send_json_sync(tr50, "thing.attr.get", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	} else {
		*value = (char *)tr50_json_get_object_item_as_string(reply, "value");
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}

	 	return 0;
	}
}

TR50_EXPORT int tr50_thing_attr_unset_ex(void *tr50, const char *key, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;

	if (key == NULL) {
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "key", key);

	if ((ret = send_json_sync(tr50, "thing.attr.unset", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_thing_bind_ex(void *tr50, const char *thing_key, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;

	if (thing_key == NULL) {
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "key", thing_key);

	if ((ret = send_json_sync(tr50, "thing.bind", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_thing_unbind_ex(void *tr50, const char *thing_key, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;

	if (thing_key == NULL) {
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tr50_json_add_string_to_object(params, "key", thing_key);

	if ((ret = send_json_sync(tr50, "thing.unbind", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_thing_tag_add_ex(void *tr50, const char *tags[], const int count, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;
	JSON *tags_array;

 	if (tags == NULL) {
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tags_array = tr50_json_create_string_array(tags, count);
	tr50_json_add_item_to_object(params, "tags", tags_array);

	if ((ret = send_json_sync(tr50, "thing.tag.add", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_thing_tag_delete_ex(void *tr50, const char *tags[], const int count, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;
	JSON *tags_array;

	if (tags == NULL) {
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = (JSON *)optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}

	tags_array = tr50_json_create_string_array(tags, count);
	tr50_json_add_item_to_object(params, "tags", tags_array);

	if ((ret = send_json_sync(tr50, "thing.tag.delete", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}

	return 0;
}

TR50_EXPORT int tr50_file_get_ex(void *tr50, const char *filename, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;

	if (filename == NULL) {
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = (JSON *)optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}
	tr50_json_add_string_to_object(params, "fileName", filename);

	if ((ret = send_json_sync(tr50, "file.get", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = reply;
		} else if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}
	if (optional_reply) {
		*optional_reply = reply;
	} else if (reply) {
		tr50_json_delete(reply);
	}


	return 0;
}

TR50_EXPORT int tr50_file_put_ex(void *tr50, const char *filename, void *optional_params, void **optional_reply, char **error_msg) {
	JSON *reply = NULL;
	int ret;
	JSON *params = NULL;
 
	if (filename == NULL) {
		if (optional_params) tr50_json_delete(optional_params);
		return ERR_TR50_PARMS;
	}

	if (optional_params != NULL) {
		params = (JSON *)optional_params;
	} else {
		if ((params = tr50_json_create_object()) == NULL) {
			return ERR_TR50_MALLOC;
		}
	}
    tr50_json_add_string_to_object(params, "fileName", filename);
 
	if ((ret = send_json_sync(tr50, "file.put", params, &reply, error_msg)) != 0) {
		if (optional_reply) {
			*optional_reply = NULL;
		}
		if (reply) {
			tr50_json_delete(reply);
		}
		return ret;
	}
	if (optional_reply) {
		*optional_reply = reply;
	} else if (reply) {
		tr50_json_delete(reply);
	}

	return 0;
}
 

int tr50_helper_http_header_msg_decoder(void* socket, int* is_chunked, long long* file_size) {
	int ret = 0;
	void *blob = NULL;
	char  buffer;
	int termchr = 0;
	char * tmpbuf = NULL;
	_blob_create(&blob, 512);
	_blob_clear(blob);
	int tmplen = 1;
	while (1) {
		buffer = 0;
		if ((ret = _tcp_recv(socket, &buffer, &tmplen, 5000)) != 0) {
			break;
		}
		_blob_append(blob, &buffer, 1);
		if (buffer != '\r' &&  buffer != '\n') {
			termchr = 0;
			continue;
		}
		++termchr;
		if (termchr == 4) break;
	}
	if (strncmp((char*)_blob_get_buffer(blob), "HTTP/1.1 200", 12) != 0) {
		_blob_delete(blob);
		return ret;
	}
	if (file_size != NULL) {
		tmpbuf = strstr((char*)_blob_get_buffer(blob), "Content-Length: ");
		if ((tmpbuf != NULL) && ((tmpbuf = strtok(tmpbuf + 16, "\r\n")) != NULL)) {
			*file_size = strtol((char*)tmpbuf, NULL, 16);
		} else {
			*file_size = -1;
		}
	}
	if (is_chunked != NULL) {
		if ((tmpbuf = strstr((char*)_blob_get_buffer(blob), "Transfer-Encoding: chunked\r\n")) != NULL) {
			*is_chunked = 1;
		} else {
			*is_chunked = 0;
		}
	}
	_blob_delete(blob);
	return 0;
}

int tr50_helper_reader(void* socket, FILE* fp, int filesize) {
	int ret = 0;
	int data_buffer_len = 0;
	void* data = NULL;
	int termchr = 0;
	char stream[READ_SIZE];
	int chunk_index = 0, tmplen = 0;
	data_buffer_len = filesize;
	if (data_buffer_len == 0) return -1;
	while (data_buffer_len > 0) {
		tmplen = ((data_buffer_len > 1024) ? 1024 : data_buffer_len);
		ret = _tcp_recv(socket, stream, &tmplen, 5000);
		data_buffer_len -= tmplen;
		if (ret != 0) return ret;
		if (fwrite(stream, tmplen, 1, fp) <= 0) {
			fclose(fp);
			return ERR_TR50_FILE_WRITE_FAILED;
		}
	}
	return ret;
}

int tr50_helper_chunked_reader(void *socket, FILE* fp) {
	int ret = 0;
	int data_buffer_len = 0;
	void* data = NULL;
	char  buffer;
	int termchr = 0;
	char chunk_stream[READ_SIZE];
	int chunk_index = 0, tmplen = 0;
	int tmp_stream = 1;

	_memory_memset(chunk_stream, 0, sizeof(char)*READ_SIZE);
	while (1) {
		buffer = 0;
		if ((ret = _tcp_recv(socket, &buffer, &tmp_stream, 5000)) != 0) {
			break;
		}
		chunk_stream[chunk_index++] = buffer;
		if (buffer != '\r' &&  buffer != '\n') {
			termchr = 0;
			continue;
		}
		++termchr;
		if (termchr == 2) break;
	}
	data_buffer_len = (int)strtol((char*)chunk_stream, NULL, 16);
	if (data_buffer_len == 0) return -1;
	while (data_buffer_len > 0) {
		tmplen = ((data_buffer_len > 1024) ? 1024 : data_buffer_len);
		ret = _tcp_recv(socket, chunk_stream, &tmplen, 5000);
		data_buffer_len -= tmplen;
		if (ret != 0) return ret;
		if (fwrite(chunk_stream, tmplen, 1, fp) <= 0) {

			return ERR_TR50_FILE_WRITE_FAILED;
		}
	}
	return ret;
}

TR50_EXPORT int tr50_helper_file_upload(void* tr50, const char *thing_key, const char *src, const char *dest, const char *tags, char* is_public, char **error_msg, int is_global, int log_completion) {
	int ret;
	void * json_reply_optional = NULL;
	JSON *optional_params = NULL;
	const char *file_id;
	FILE *fi;
	//connect vars
	void* socket = NULL;
	char  tmphost[128];
	char stream[READ_SIZE];
	char chunk_header[24];
	int bytes_read = 0;

	if ((fi = fopen(src, "rb")) == NULL) {
		return ERR_TR50_LOCAL_FILE_NOTFOUND;
	}
	if ((optional_params = tr50_json_create_object()) == NULL) {
		return ERR_TR50_MALLOC;
	}

	tr50_json_add_bool_to_object(optional_params, "global", is_global);
	tr50_json_add_bool_to_object(optional_params, "logComplete", log_completion);

	if ((!is_global) && thing_key) {
		tr50_json_add_string_to_object(optional_params, "thingKey", thing_key);
	}

	if (tags) {
		char *token = NULL, *reent = NULL;
		char *tags_clone = NULL;
		JSON *tags_json = tr50_json_create_array();
		tags_clone = _memory_clone((char*)tags, strlen(tags));
		if (tags_clone != NULL) {
			token = strtok_r(tags_clone, ",", &reent);
		}
		while (token) {
			tr50_json_add_item_to_array(tags_json, tr50_json_create_string(token));
			token = strtok_r(NULL, ",", &reent);
		}
		if (tags_clone) _memory_free(tags_clone);
		tr50_json_add_item_to_object(optional_params, "tags", tags_json);
	}
	if (is_public != NULL) {
		tr50_json_add_bool_to_object(optional_params, "public", *is_public);
	}

	if ((ret = tr50_file_put_ex(tr50, dest, optional_params, &json_reply_optional, error_msg)) != 0) {
		goto _end_err;
	}
	if ((json_reply_optional == NULL || ((file_id = tr50_json_get_object_item_as_string(json_reply_optional, "fileId")) == NULL))) {
		ret = ERR_TR50_PARMS;
		goto _end_err;
	}
	snprintf(stream, 1024, POST_COMMAND, file_id, tr50_config_get_host(tr50));
	snprintf(tmphost, 128, "%s:80", tr50_config_get_host(tr50));
	if ((ret = _tcp_connect(&socket, tmphost, 80, 0)) != 0) goto _end_err;
	if ((ret = _tcp_send(socket, stream, strlen(stream), 5000)) != 0) goto _end_err;
	while ((bytes_read = fread(stream, 1, 1024, fi)) > 0) {
		snprintf(chunk_header, 24, "%x\r\n", bytes_read);
		if ((ret = _tcp_send(socket, chunk_header, strlen(chunk_header), 5000)) != 0) goto _end_err;
		if ((ret = _tcp_send(socket, stream, bytes_read, 5000)) != 0) {
			goto _end_err;
		}
		if ((ret = _tcp_send(socket, "\r\n", 2, 5000)) != 0) goto _end_err;
	}
	if ((ret = _tcp_send(socket, "0\r\n\r\n", 5, 5000)) != 0) goto _end_err;
	if ((ret = tr50_helper_http_header_msg_decoder(socket, NULL, NULL)) != 0) goto _end_err;
	

_end_err:
	if(fi) fclose(fi);
	if (ret == -1) {
		ret = ERR_TR50_FILE_PUT_HTTP;
	}
	if(socket) _tcp_disconnect(socket); //shutdown//close
	if (json_reply_optional) _memory_free(json_reply_optional);
	return ret;
}

TR50_EXPORT int tr50_file_download(void* tr50, const char *thing_key, const char *src, const char *dest, char **error_msg, int is_global) {
	JSON *optional_params;
    void * json_reply_optional = NULL;

	const char *file_id;
	FILE *fp = NULL;
	//file vars
	void* socket = NULL;
	char  tmphost[128];
	char stream[READ_SIZE];
	int ret = 0;
	int is_chunked = 0;
	long long filesize = 0;
	if ((optional_params = tr50_json_create_object()) == NULL) {
		return ERR_TR50_MALLOC;
	}
	tr50_json_add_string_to_object(optional_params, "fileName", src);
	tr50_json_add_bool_to_object(optional_params, "global", is_global);
	if ((!is_global) && thing_key) {
		tr50_json_add_string_to_object(optional_params, "thingKey", thing_key);
	}
	if ((ret = tr50_file_get_ex(tr50, src, optional_params, &json_reply_optional, error_msg)) != 0) {
		goto _end_err_get;
	}
	if ((json_reply_optional == NULL || ((file_id = tr50_json_get_object_item_as_string(json_reply_optional, "fileId")) == NULL))) {
		ret = ERR_TR50_PARMS;
		if (json_reply_optional) _memory_free(json_reply_optional);
		return ret;
	}

	if ((fp = fopen(dest, "wb")) == NULL) {
		return ERR_TR50_LOCAL_FILE_NOTFOUND;
	}
	snprintf(stream, 1024, GET_COMMAND, file_id, tr50_config_get_host(tr50));
	snprintf(tmphost, 128, "%s", tr50_config_get_host(tr50));
	if ((ret = _tcp_connect(&socket, tmphost, 80, 0)) != 0) goto _end_err_get;
	if ((ret = _tcp_send(socket, stream, strlen(stream), 5000)) != 0) goto _end_err_get;
	ret = tr50_helper_http_header_msg_decoder(socket, &is_chunked, &filesize);
	if (is_chunked) {
		do {
			ret = tr50_helper_chunked_reader(socket, fp);
		} while (ret == 0);
	} else {
		ret = tr50_helper_reader(socket, fp, filesize);
	}
_end_err_get:
	if (fp) fclose(fp);
	if ((ret == -1) || (ret == 0)) {
		ret = 0;
	} else {
		ret = ERR_TR50_FILE_PUT_HTTP;
	}
	if(socket) _tcp_disconnect(socket);
	if (json_reply_optional) _memory_free(json_reply_optional);
	return ret;
}
 
 
