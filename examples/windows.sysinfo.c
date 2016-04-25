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
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <Psapi.h>
#include <direct.h>

#include <tr50/tr50.h>
#include <tr50/worker.h>

void *tr50;
#define ARG_MQTT_ENDPOINT 1
#define ARG_MQTT_PORT 2
#define ARG_APP_TOKEN 3
#define ARG_THING_KEY 4
#define SSL_PORT 8883
#define C_Drive 3
#define OVERTEMP "overheating"
int _run_method(const char* thing_key);
int _method_exec_overtemp_callback(void* tr50, const char *id, const char *thing_key, const char *command, const char *from, JSON *params, void* custom);
int _publish_name();
int _publish_memory();
int _publish_disk();

int main(int argc, char *argv[]) {
	int ret;

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
 
	_publish_name();
	_run_method(argv[ARG_THING_KEY]);
	while (1) {
  		_publish_memory();
		_publish_disk();
		Sleep(5000);
	}

	return 0;
}
/*********************************************************************************
* Set an attribute and retrieve it back from the cloud using set and get helpers *
*********************************************************************************/
int _publish_name() {
	int ret;
	LPSTR name;
	char * name_reply;
	JSON* reply = NULL;
	char* err_msg = NULL;
	int name_len = MAX_COMPUTERNAME_LENGTH;
	name = malloc(MAX_COMPUTERNAME_LENGTH+1);
	
	GetComputerNameA(name,&name_len);
	//set key value
 	ret = tr50_thing_attr_set(tr50, "name", (const char *)name);
	printf("tr50_thing_attr_set(): key[%s] value[%s] ret[%d]\n", "name", name, ret);

	//get key value
	ret = tr50_thing_attr_get_ex(tr50, "name", &name_reply, NULL,  &reply, &err_msg);
	if (ret)  {
		 printf("ERROR MESSAGE:[%s]\n", err_msg);
	}
	else {
		//print returned "value"
		printf("tr50_thing_attr_get() value: key[%s] value[%s] ret[%d]\n", "name", name_reply, ret);

		//get and print value from reply msg
		name_reply = (char *)tr50_json_get_object_item_as_string( reply, "value");
		printf("tr50_thing_attr_get() params: key[%s] value[%s] ret[%d]\n", "name", name_reply, ret);
	}
	if (reply != NULL)  {
		tr50_json_delete(reply);
	}
 	free(name);  
	return 0;
}

int _publish_memory() {
	int ret;
	double value;

	MEMORYSTATUS ms;
	GlobalMemoryStatus(&ms);
	value = (double)ms.dwMemoryLoad;

	ret = tr50_property_publish(tr50,"memory_usage",value);
	printf("tr50_property_publish(): key[%s] value[%f] ret[%d]\n", "memory_usage", value, ret);
 	return 0;
} 

int _run_method(const char* arg_thing_key)  {
 	JSON* param =NULL;
	JSON* optional_params=NULL;
	int id = 0;
	param = tr50_json_create_object();

	optional_params = tr50_json_create_object();
	tr50_json_add_string_to_object(param, "paramsKey", "85");

	tr50_json_add_string_to_object(optional_params, "thingKey", arg_thing_key);
	tr50_method_exec_ex_sync(tr50, OVERTEMP, param,optional_params,  NULL);
 
	return 0;
}

int _publish_disk() {
	int free_kb, total_kb;
	double value;
	int ret;
	struct _diskfree_t df = {0};
	unsigned uErr;

	if( (uErr=_getdiskfree(C_Drive, &df))!=0) {  
		printf("statvfs() failed: %d\n", uErr);
	} else {
		long long byte_per_cluster = df.sectors_per_cluster;
		byte_per_cluster*= df.bytes_per_sector;
		
		free_kb = (int)(df.avail_clusters*byte_per_cluster/1024);
		total_kb = (int)(df.total_clusters*byte_per_cluster/1024);

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
