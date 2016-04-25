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


#if defined (_UNIX)
#include <unistd.h>
#elif defined (_WIN32)
#include <Windows.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <tr50/tr50.h>
#include <tr50/worker.h>

#define ARG_MQTT_ENDPOINT 1
#define ARG_MQTT_PORT 2
#define ARG_APP_TOKEN 3
#define ARG_THING_KEY 4
#define SSL_PORT 8883
/***************************************************************************/
/* Helper that calls tr50 to create session and connect to server          */
/***************************************************************************/
int create_session_and_connect(void** tr50, const char* client_id, const char* host, int port, const char* app_token) {
	int ret;
	// Setup the connection to the TR50 server.
	ret = tr50_create(tr50, client_id, host, port);
	if (ret != 0) {
		printf("tr50_create(): ERROR [%d]\n", ret);
		return 1;
	} else {
		printf("tr50_create(): OK\n");
	}
	if (port == SSL_PORT) {
		tr50_config_set_ssl(*tr50, 1);
	}
    tr50_config_set_username(*tr50, client_id);
	tr50_config_set_password(*tr50, app_token);

	// Connect to the TR50 server.
 	if (tr50_start(*tr50) != 0) {
		printf("tr50_start(): ERROR [%d]\n", ret);
		return 1;
	} else {
		printf("tr50_start(): OK\n");
	}
	return 0;
}
/***************************************************************************/
/* Connect TR50, set an attribute, publish properties and publish an alarm.*/
/***************************************************************************/
int main(int argc, char *argv[]) {
	int ret;
	double value;
	int rand_value;
	int state;
	void * tr50;
	
	// Uncomment the following line to enable low-level debug logging. 
	// For more info, refer to log.h file
	//tr50_log_set_low_level();

	if(argc!=5) {
		printf("Usage: example_basic [mqtt_endpoint] [mqtt_port] [app_token] [thing_key]\n");
		return 1;
	}
	printf("Connecting to TR50..\n");
	// Create the TR50 session handle (tr50) and setup the connection to the TR50 server.
	ret = create_session_and_connect(&tr50, argv[ARG_THING_KEY], argv[ARG_MQTT_ENDPOINT], atoi(argv[ARG_MQTT_PORT]), argv[ARG_APP_TOKEN]);
	if(ret!=0) {
		printf("tr50_connect(): ERROR [%d]\n", ret);
		return 1;
	} else {
		printf("tr50_connect(): OK\n");
	}
  
	ret=tr50_thing_attr_set(tr50, "attr1", "attr_value");
	printf("tr50_thing_attr_set(): key[%s] value[%s] ret[%d]\n","attr1","attr_value",ret);

	while (1) {
		rand_value = rand();
		value = (double)(rand_value % 1000) / 10.0;
		ret = tr50_property_publish(tr50, "prop1", value);
		printf("tr50_property_publish(): key[%s] value[%f] ret[%d]\n", "prop1", value, ret);

		rand_value = rand();
		value = (double)(rand_value % 1000) / 10.0;
		ret = tr50_property_publish(tr50, "prop2", value);
		printf("tr50_property_publish(): key[%s] value[%f] ret[%d]\n", "prop2", value, ret);

		rand_value = rand();
		state = (rand_value % 3);
		ret = tr50_alarm_publish(tr50, "alarm1", state);
		printf("tr50_alarm_publish(): key[%s] state[%d] ret[%d]\n", "alarm1", state, ret);
#if defined (_UNIX)
		sleep(10000);
#elif defined (_WIN32)
		Sleep(10000);
#endif
	}


	return 0;
}
 
