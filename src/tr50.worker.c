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

#include <tr50/worker.h>
#include <tr50/tr50.h>
#include <tr50/util/memory.h>

#define TR50_MAILBOX_SEND_TIMEOUT_BUFFER 5000
#define TR50_METHOD_TIMEOUT_BUFFER	5000

TR50_EXPORT int tr50_alarm_publish(void *tr50, const char *alarm_key, const int state) 
{
	int ret;
	ret = tr50_alarm_publish_ex(tr50, alarm_key, state, NULL, NULL, NULL);
	return ret;
}

TR50_EXPORT int tr50_location_publish(void *tr50,  const double lat_value, const double long_value){
	int ret;
	ret = tr50_location_publish_ex(tr50, lat_value, long_value, NULL, NULL, NULL);
	return ret;
}


TR50_EXPORT int tr50_log_publish(void *tr50, const char *msg) {
	int ret;
	ret = tr50_log_publish_ex(tr50, msg, NULL, NULL, NULL);
	return ret;
}

TR50_EXPORT int tr50_mailbox_send(void *tr50, const char *mailbox_cmd, JSON *req_params) {
	int ret=1;
	ret = tr50_mailbox_send_ex(tr50, mailbox_cmd, req_params, NULL, NULL, NULL);
	return ret;
}
TR50_EXPORT int tr50_method_exec(void *tr50, const char *method, JSON *req_params) {
	int ret;
	ret = tr50_method_exec_ex_sync(tr50, method, req_params, NULL,  NULL);
	return 0;
}

TR50_EXPORT int tr50_property_publish(void *tr50, const char *prop_key, const double value) {
	int ret;
	ret = tr50_property_publish_ex(tr50, prop_key, value, NULL, NULL, NULL);
	return ret;
}

TR50_EXPORT int tr50_property_current(void *tr50, const char *prop_key, double *value) {
	int ret;
	ret = tr50_property_current_ex(tr50, prop_key, value, NULL, NULL, NULL);
	return ret;
 }

TR50_EXPORT int tr50_thing_attr_set(void *tr50, const char *key, const char *value) {
	int ret;
	ret = tr50_thing_attr_set_ex(tr50, key, value, NULL, NULL, NULL);
	return ret;
}

TR50_EXPORT int tr50_thing_attr_get(void *tr50,  const char *key, char **value) {
	int ret;
 
	ret = tr50_thing_attr_get_ex(tr50, key, value, NULL, NULL, NULL);
	return ret;
 
}

TR50_EXPORT int tr50_thing_attr_unset(void *tr50, const char *key) {
	int ret;
	ret = tr50_thing_attr_unset_ex(tr50, key, NULL, NULL, NULL);
	return ret;
}

TR50_EXPORT int tr50_thing_bind(void *tr50, const char *thing_key){
	int ret;
	ret = tr50_thing_bind_ex(tr50, thing_key, NULL, NULL, NULL);
	return ret;
}

TR50_EXPORT int tr50_thing_unbind(void *tr50, const char *thing_key) {
	int ret;
	ret = tr50_thing_unbind_ex(tr50, thing_key, NULL, NULL, NULL);
	return ret;
}

TR50_EXPORT int tr50_thing_tag_add(void *tr50, const char *tags[],	const int numtags) {
	int ret;
	ret = tr50_thing_tag_add_ex(tr50, tags, numtags, NULL, NULL, NULL);
	return ret;
}

TR50_EXPORT int tr50_thing_tag_delete(void *tr50, const char *tags[], const int numtags) {
	int ret ; 
	ret = tr50_thing_tag_delete_ex(tr50, tags, numtags, NULL, NULL, NULL);
 	return ret;
}

