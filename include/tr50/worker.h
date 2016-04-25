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

#ifndef _WORKER_H_
#define _WORKER_H_

#include <stddef.h>
#include <tr50/util/json.h>
#include <tr50/tr50.h>
#include <tr50/error.h>
#include <tr50/util/memory.h>
#include <tr50/util/time.h>
#if defined(_WIN32)
#  if defined(EXPORT_TR50_SYMS)
#    define TR50_EXPORT __declspec(dllexport)
#  else
#    define TR50_EXPORT __declspec(dllimport)
#  endif
#else
#  define TR50_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
 
int send_json(void *tr50, const char *cmd, JSON *params, JSON **reply_params, char** error_msg);
int send_json_async(void *tr50, const char *cmd, JSON *params, int* id, tr50_async_reply_callback callback, void *custom_cb_obj, int timeout);
int send_json_sync(void *tr50, const char *cmd, JSON *params, JSON **reply_params, char** error_msg);
TR50_EXPORT int tr50_alarm_publish(void *tr50, const char * alarm_key, const int state);
TR50_EXPORT int tr50_property_publish(void *tr50, const char *prop_key, const double value);
TR50_EXPORT int tr50_property_current(void *tr50, const char *prop_key, double *value);
TR50_EXPORT int tr50_location_publish(void *tr50, const double lat, const double lon);
TR50_EXPORT int tr50_log_publish(void *tr50, const char *msg);
TR50_EXPORT int tr50_thing_bind(void *tr50, const char *thing_key);
TR50_EXPORT int tr50_thing_unbind(void *tr50, const char *thing_key);
TR50_EXPORT int tr50_thing_attr_set(void *tr50, const char *key, const char* value);
TR50_EXPORT int tr50_thing_attr_unset(void *tr50, const char *key);
TR50_EXPORT int tr50_thing_attr_get(void *tr50, const char *key, char **value);
TR50_EXPORT int tr50_thing_tag_add(void *tr50, const char *tags[], const int  count);
TR50_EXPORT int tr50_thing_tag_delete(void *tr50, const char *tags[], const int count);
TR50_EXPORT int tr50_mailbox_send(void *tr50, const char *mailbox_cmd, JSON* params);
TR50_EXPORT int tr50_method_exec(void *tr50, const char *method, JSON* params);

//extended
TR50_EXPORT int tr50_alarm_publish_ex(void *tr50, const char * alarm_key, const int state, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_property_publish_ex(void *tr50, const char *prop_key, const double value, void* optional_params,  void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_property_current_ex(void *tr50, const char *prop_key, double *value, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_location_publish_ex(void *tr50, const double lat, const double lon, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_log_publish_ex(void *tr50, const char * msg, void* optional_params, void** optional_reply,char** error_msg);
TR50_EXPORT int tr50_thing_bind_ex(void *tr50, const char *thing_key, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_thing_unbind_ex(void *tr50, const char *thing_key, void* optional_params, void** optinal_reply, char** error_msg);
TR50_EXPORT int tr50_thing_attr_set_ex(void *tr50, const char *key, const char *value, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_thing_attr_unset_ex(void *tr50, const char *key, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_thing_attr_get_ex(void *tr50, const char *key, char **value, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_thing_tag_add_ex(void *tr50, const char *tags[], const int count, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_thing_tag_delete_ex(void *tr50, const char *tags[], const int count, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_mailbox_send_ex(void *tr50, const char *command, JSON* req_params, void* optional_params, void** reply_params, char** error_msg);
TR50_EXPORT int tr50_method_exec_ex_async(void *tr50, const char *method, int* id, JSON* req_params, void* optional_params, tr50_async_reply_callback callback, void *custom_cb_obj, int timeout);
TR50_EXPORT int tr50_method_exec_ex_sync(void *tr50, const char *method, JSON* req_params, void* optional_params, void** optional_reply);
TR50_EXPORT int tr50_file_put_ex(void *tr50, const char *filename, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_file_get_ex(void *tr50, const char *filename, void* optional_params, void** optional_reply, char** error_msg);
TR50_EXPORT int tr50_file_download(void* tr50, const char *thing_key, const char *src, const char *dest, char **error_msg, int is_global);
#ifdef __cplusplus
}
#endif

#endif // !_WORKER_H_
