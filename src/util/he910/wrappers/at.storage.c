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
#include <stdio.h>
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

typedef struct {
	int		id;
	char 	*output_str;
	int		output_len;
	int		is_used;
	int		mode;
	int		status;
} _AT_STORAGE;

#define TR50_AT_STORAGE_SIZE	20

_AT_STORAGE g_tr50_at_storage[TR50_AT_STORAGE_SIZE];
int g_tr50_at_storage_count = 0;

int _tr50_at_storage_put(void *message, int mode, int id, int *len) {
	int i;
 
	char *error_message = NULL;
 
	log_debug("_tr50_at_storage_put(): id[%d] ...", id);

	if (g_tr50_at_storage_count > TR50_AT_STORAGE_SIZE) {
		return ERR_TR50_AT_STORAGE_FULL;
	}

	for (i = 0; i < TR50_AT_STORAGE_SIZE; ++i) {
		if (g_tr50_at_storage[i].is_used) {
			continue;
		}
		//new mode timeout and when it comes out put a timeout message
		if (mode == TR50_AT_SEND_MODE_METHOD) {
			g_tr50_at_storage[i].status = 0;
			g_tr50_at_storage[i].output_str = message;
			g_tr50_at_storage[i].output_len = strlen(message);
 
		} else if (tr50_reply_get_error(message, "1", &error_message) != 0) {
		  
			 
			g_tr50_at_storage[i].status = 1; //tr50_message_get_status(message);
 

			if (error_message) {
				int msg_len = strlen(error_message);
				g_tr50_at_storage[i].output_str = (char *)_memory_malloc(32 + msg_len);
				snprintf(g_tr50_at_storage[i].output_str, 32 + msg_len, "Failed: %s", error_message);
				_memory_free(error_message);
			} else {
				g_tr50_at_storage[i].output_str = (char *)_memory_malloc(32);
				snprintf(g_tr50_at_storage[i].output_str, 32, "Failed without message.");
			}
			g_tr50_at_storage[i].output_len = strlen(g_tr50_at_storage[i].output_str);//
		} else if (mode == TR50_AT_SEND_MODE_DELIMITED) {
			void *blob;
			JSON *params, *ptr;

			_blob_create(&blob, 256);
 
			if ((params = tr50_reply_get_params(message, "1", FALSE)) && (ptr = params->child)) {
 
				while (ptr) {
					switch (ptr->type) {
					case JSON_FALSE:
						_blob_format_append(blob, "%s,false", ptr->string);
						break;
					case JSON_TRUE:
						_blob_format_append(blob, "%s,true", ptr->string);
						break;
					case JSON_NUMBER: {
						int i = ptr->valuedouble;
						if (ptr->valuedouble == ((double)i)) {
							_blob_format_append(blob, "%s,%d", ptr->string, ptr->valueint);
						} else {
							_blob_format_append(blob, "%s,%lf", ptr->string, ptr->valuedouble);
						}
						break;
					}
					case JSON_STRING:
						_blob_format_append(blob, "%s,%s", ptr->string, ptr->valuestring);
						break;
					default:
						break;
					}
					if ((ptr = ptr->next)) {
						_blob_format_append(blob, ",");
					}
				}
			} else {
				_blob_format_append(blob, "OK");
			}
			g_tr50_at_storage[i].status = 0;
			g_tr50_at_storage[i].output_str = (char *)_blob_get_buffer(blob);
			g_tr50_at_storage[i].output_len = _blob_get_length(blob);
			_blob_delete_object(blob);
		} else if (mode == TR50_AT_SEND_MODE_RAW) {
			g_tr50_at_storage[i].status = 0;
			tr50_message_to_string("reply", message, &g_tr50_at_storage[i].output_str, &g_tr50_at_storage[i].output_len);
		} else {
			return ERR_TR50_AT_SEND_MODE_UNKNOWN;
		}
		g_tr50_at_storage[i].mode = mode;
		g_tr50_at_storage[i].is_used = 1;
		g_tr50_at_storage[i].id = id;
		++g_tr50_at_storage_count;

		*len = g_tr50_at_storage[i].output_len;
		log_debug("_tr50_at_storage_put(): id[%d] len[%d] OK.", id, *len);
		return 0;
	}

	return ERR_TR50_AT_STORAGE_FULL;
}

int _tr50_at_storage_get(int id, int mode, int *status, char **output, int *output_len) {
	int i;
	for (i = 0; i < TR50_AT_STORAGE_SIZE; ++i) {
		if (!g_tr50_at_storage[i].is_used) {
			log_debug("_tr50_at_storage_get(): index[%d] is not used.", i);
			continue;
		}
		if (g_tr50_at_storage[i].id != id) {
			log_debug("_tr50_at_storage_get(): index[%d] is not equal to [%d].", i, mode);
			continue;
		}
		if (g_tr50_at_storage[i].mode != mode && (g_tr50_at_storage[i].mode == TR50_AT_SEND_MODE_METHOD && mode != TR50_AT_SEND_MODE_DELIMITED)) {
			log_debug("_tr50_at_storage_get(): index[%d] is mode mismatch.", i);
			continue;
		}
		*status = g_tr50_at_storage[i].status;
		*output = g_tr50_at_storage[i].output_str;
		*output_len = g_tr50_at_storage[i].output_len;
		g_tr50_at_storage[i].is_used = 0;
		--g_tr50_at_storage_count;
		return 0;
	}
	return ERR_TR50_AT_STORAGE_NOTFOUND;
}

void _tr50_at_storage_list(void *blob) {
	int i;
	_blob_format_append(blob, "%d", g_tr50_at_storage_count);
	for (i = 0; i < TR50_AT_STORAGE_SIZE; ++i) {
		if (!g_tr50_at_storage[i].is_used) {
			continue;
		}
		_blob_format_append(blob, ",%d,%d", g_tr50_at_storage[i].id, g_tr50_at_storage[i].output_len);
	}
}

int _tr50_at_storage_is_full() {
	// Default to LIFO
	// return (g_tr50_at_storage_count >= TR50_AT_STORAGE_SIZE);
	return 0;
}
