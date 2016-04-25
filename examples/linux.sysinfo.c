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

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/reboot.h>


#include <tr50/tr50.h>

void *tr50;
#define ARG_MQTT_ENDPOINT 1
#define ARG_MQTT_PORT 2
#define ARG_APP_TOKEN 3
#define ARG_THING_KEY 4
#define SSL_PORT 8883
#define OVERTEMP "overheating"
int _run_method(const char* thing_key);
int _method_exec_overtemp_callback(void* tr50, const char *id, const char *thing_key, const char *command, const char *from, JSON *params, void* custom);
int _publish_name();
int _publish_memory();
int _publish_disk();
int main(int argc, char *argv[]) {
	int ret;
	double value;
	int rand_value;
	int state;

	// Uncomment the following line to enable low-level debug logging. 
	// For more info, refer to log.h file
	//tr50_log_set_low_level();

	if (argc != 5) {
		printf("Usage: example_sysinfo [mqtt_endpoint] [mqtt_port] [app_token] [thing_key]\n");
		return 1;
	}

	// Setup the connection to the TR50 server.
	ret = tr50_create(&tr50, argv[ARG_THING_KEY], argv[ARG_MQTT_ENDPOINT], atoi(argv[ARG_MQTT_PORT]));
	if (ret != 0) {
		printf("tr50_create(): ERROR [%d]\n", ret);
		return 1;
	} else {
		printf("tr50_create(): OK\n");
	}

	tr50_method_register(tr50, OVERTEMP, _method_exec_overtemp_callback);

	if (atoi(argv[ARG_MQTT_PORT]) == SSL_PORT) {
		tr50_config_set_ssl(tr50, 1);
	}
	tr50_config_set_username(tr50, argv[ARG_THING_KEY]);
	tr50_config_set_password(tr50, argv[ARG_APP_TOKEN]);

	// Connect to the TR50 server.
	ret = tr50_start(tr50);
	if (ret != 0) {
		printf("tr50_start(): ERROR [%d]\n", ret);
		return 1;
	} else {
		printf("tr50_start(): OK\n");
	}

	_publish_uname();
	_run_method(argv[ARG_THING_KEY]);
	while (1) {
		_publish_sysinfo();
		_publish_statvfs();

		sleep(5);
	}

	return 0;
}

int _publish_uname() {
	struct utsname u;
	int ret;

	uname(&u);

	ret = tr50_thing_attr_set(tr50, "sysname", u.sysname);
	printf("tr50_thing_attr_set(): key[%s] value[%s] ret[%d]\n", "sysname", u.sysname, ret);

	ret = tr50_thing_attr_set(tr50,"release", u.release);
	printf("tr50_thing_attr_set(): key[%s] value[%s] ret[%d]\n", "release", u.release, ret);

	ret = tr50_thing_attr_set(tr50,"version", u.version);
	printf("tr50_thing_attr_set(): key[%s] value[%s] ret[%d]\n", "version", u.version, ret);

	ret = tr50_thing_attr_set(tr50, "machine", u.machine);
	printf("tr50_thing_attr_set(): key[%s] value[%s] ret[%d]\n", "machine", u.machine, ret);

	return 0;
}

int _run_method(const char* arg_thing_key) {
	JSON* param = NULL;
	JSON* optional_params = NULL;
	int id = 0;
	param = tr50_json_create_object();

	optional_params = tr50_json_create_object();
	tr50_json_add_string_to_object(param, "paramsKey", "85");

	tr50_json_add_string_to_object(optional_params, "thingKey", arg_thing_key);
	tr50_method_exec_ex_sync(tr50, OVERTEMP, param, optional_params, NULL);

	return 0;
}

int _publish_sysinfo() {
	struct sysinfo info;
	int ret;
	double value;

	if ((ret = sysinfo(&info)) == 0) {
		long long total = (long long)info.totalram * info.mem_unit + (long long)info.totalswap * info.mem_unit;
		long long used = (long long)(info.totalram - info.freeram) * info.mem_unit + (long long)(info.totalswap - info.freeswap) * info.mem_unit;
		int usage = used * 100 / total;

		value = (double)usage;
		ret = tr50_property_publish(tr50,"memory_usage",value);
		printf("tr50_property_publish(): key[%s] value[%f] ret[%d]\n", "memory_usage", value, ret);

		value = (double)info.procs;
		ret = tr50_property_publish(tr50, "running_procs",value);
		printf("tr50_property_publish(): key[%s] value[%f] ret[%d]\n", "running_procs", value, ret);
	}

	return 0;
}

int _publish_statvfs() {
	struct statvfs stvfs;
	long long free_kb, total_kb;
	double value;
	int ret;

	memset(&stvfs, 0, sizeof(struct statvfs));

	if (statvfs("/", &stvfs) != 0) {
		printf("statvfs() failed: errno: %s", strerror(errno));
		return 1;
	} else {
		free_kb = stvfs.f_bsize;
		free_kb *= stvfs.f_bfree;
		free_kb /= 1024;
		total_kb = stvfs.f_bsize;
		total_kb *= stvfs.f_blocks;
		total_kb /= 1024;

		value = (double)(100.0 - (free_kb * 100.0 / total_kb));
		ret = tr50_property_publish(tr50, "disk_usage", value );
		printf("tr50_property_publish(): key[%s] value[%f] ret[%d]\n", "disk_usage", value, ret);

	}
	return 0;
}

int _method_exec_overtemp_callback(void* tr50, const char *id, const char *thing_key, const char *method, const char *from, JSON *params, void* custom) {
	char *json = tr50_json_print(params);
	const char *parameter1;

	tr50_method_update(tr50, id, "in progress");

	parameter1 = tr50_json_get_object_item_as_string(params, "paramsKey");
	printf("Received method callback for %s\n", method);
	printf("\a\a\a");
	if (strcmp(parameter1, "85") == 0) {
		//do something for 85deg
		printf("Received method from [%s] with params at 85 degrees: [%s]\n", from, json);
	} else {
		//otherwise
		printf("Received method from [%s] with params: [%s]\n", from, json);
	}
	tr50_method_ack(tr50, id, 0, NULL, NULL);
	return 0;
}
 