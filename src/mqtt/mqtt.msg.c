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

#include <tr50/mqtt/mqtt.h>

#include <tr50/util/memory.h>
#include <tr50/util/platform.h>

int _mqtt_decode_header_length(char *ptr, long ptr_len, int *encoded_size, int *decoded_len) {
	char d;
	int  multiplier = 1;
	long rlLength;

	*decoded_len = 0;
	rlLength = 0;

	do {
		rlLength++;		/* Increment the byte count for the remaining length */
		d = *ptr;       /* point at the next byte */
		*decoded_len += (d & 127) * multiplier;
		multiplier *= 128;
		ptr++;
	} while (((d & 128) != 0) && ((rlLength <= ptr_len) && rlLength < 4));

	/* The length is encoded in more bytes than we have been given */
	if ((d & 128) != 0) {
		*decoded_len = 0;
		return -1;
	}
	*encoded_size = rlLength;
	return 0;
}

/* Encode the message length according to the SCADA algorithm */
int _mqtt_encode_fixed_header_len(int l, char *ptr) {
	char d;

	do {
		d = l % 128;
		l = l / 128;
		/* if there are more digits to encode, set the top bit of this digit */
		if (l > 0) {
			d = d | 0x80;
		}
		*ptr = d;
		ptr++;
	} while (l > 0);

	return 0;
}


/* A UTF encoded string is preceeded by a big-endian 16 bit length, */
/* so the outBuf must be 16 bits bigger than the data being encoded */
int _mqtt_encode_string(char *ptr, const void *data, unsigned short data_len) {
	unsigned short value;

	/* Use the socket function htons to get the length in big-endian order */
	value = swap16(data_len);

	_memory_memcpy(ptr, &value, sizeof(unsigned short));
	_memory_memcpy(ptr + 2, (void *)data, (size_t)data_len);

	return 0;
}

/* A UTF encoded string is preceeded by a big-endian 16 bit length,                     */
/* To decode the string:                                                                */
/*     1. Convert the first 16 bytes into an unsigned short in the platform endian      */
/*     2. Move the UTF string pointer forward 16 bits to point to a conventional string */
int _mqtt_decode_string(char *ptr, int *output_len, char **output) {
	unsigned short len;
	/* Use the socket function ntohs to get the length in platform endian order */
	_memory_memcpy(&len, ptr, 2);
	/* Swap the bytes, if appriopriate for the platform */
	len = swap16(len);

	*output = (char *)_memory_clone(ptr + 2, len);;
	*output_len = len;
	return 0;
}

/* define the protocol name as it appears on the wire */
#define MQTT_CONN_PROTOCOL_NAME      "MQIsdp"
#define MQTT_CONN_PROTOCOL_NAME_LEN  6
#define MQTT_CONN_PROTOCOL_VERSION_3 0x03

/* Variable header connect options */
#define MQTT_CONN_OPT_USERNAME        0x80
#define MQTT_CONN_OPT_PASSWORD        0x40
#define MQTT_CONN_OPT_WILL_RETAIN     0x20
#define MQTT_CONN_OPT_WILL            0x04
#define MQTT_CONN_OPT_CLEAN_START     0x02
#define MQTT_CONN_OPT_CLEAN_START_OFF 0xFD /* AND mask - turn off bit 1 */
#define MQTT_CONN_OPT_QOS_0           0xE7 /* AND mask - turn off bits 3 and 4 */
#define MQTT_CONN_OPT_QOS_1           0x08
#define MQTT_CONN_OPT_QOS_2           0x10

int _mqtt_msg_build_connect(const char *client_id,
							const char *username,
							const char *password,
							unsigned short keepalive,
							char **data,
							int *data_len) {
	char *msg, *ptr;
	int client_id_len = 0;
	int password_len = 0;
	int username_len = 0;
	int remaining_len = 0;
	int fixed_header_len = 0;
	int total_len = 0;
	unsigned short keepalive_swap = 0;

	// calculate lengths
	client_id_len = strlen(client_id);

	remaining_len += 12;
	remaining_len += client_id_len + 2;
	if (username) {
		username_len = strlen(username);
		remaining_len += username_len + 2;
	}
	if (password) {
		password_len = strlen(password);
		remaining_len += password_len + 2;
	}

	MQTT_CALC_FHEADER_LENGTH(remaining_len, fixed_header_len);

	total_len = remaining_len + fixed_header_len;

	if ((msg = (char *)_memory_malloc(total_len)) == NULL) {
		return ERR_TR50_MALLOC;
	}
	ptr = msg;

	// set byte1
	*ptr = 0x00 | MQTT_MSG_TYPE_CONNECT;
	++ptr;

	// Encode the message length
	_mqtt_encode_fixed_header_len(remaining_len, ptr);
	ptr = msg + fixed_header_len;

	// Variable header
	// Add the protocol name
	_mqtt_encode_string(ptr, MQTT_CONN_PROTOCOL_NAME, MQTT_CONN_PROTOCOL_NAME_LEN);
	ptr += MQTT_CONN_PROTOCOL_NAME_LEN + 2;

	// Add the protocol version
	*ptr = MQTT_CONN_PROTOCOL_VERSION_3;
	ptr++;

	// Flags(username,password,Will Retain, Will Qos, Clean Session)
	*ptr = 0;
	if (username) {
		*ptr |= MQTT_CONN_OPT_USERNAME;
	}
	if (password) {
		*ptr |= MQTT_CONN_OPT_PASSWORD;
	}
	++ptr;

	// Keep Alive timer
	keepalive_swap = swap16(keepalive);
	_memory_memcpy(ptr, &keepalive_swap, sizeof(unsigned short));
	ptr += sizeof(unsigned short);

	// Payload
	// ClientID
	_mqtt_encode_string(ptr, client_id, client_id_len);
	ptr += client_id_len + 2;

	// Username
	if (username) {
		_mqtt_encode_string(ptr, username, username_len);
		ptr += username_len + 2;
	}

	// Password
	if (password) {
		_mqtt_encode_string(ptr, password, password_len);
		ptr += password_len + 2;
	}
	*data = msg;
	*data_len = total_len;
	return 0;
}


/* Some response flags specific to CONNACK */
#define MQTT_CONNACK_ACCEPTED						0x00
#define MQTT_CONNACK_REFUSED_VERSION				0x01
#define MQTT_CONNACK_REFUSED_ID						0x02
#define MQTT_CONNACK_REFUSED_BROKER					0x03
#define MQTT_CONNACK_REFUSED_BAD_USER_PASSWORD		0x04
#define MQTT_CONNACK_REFUSED_NOT_AUTHORIZED			0x05

int mqtt_msg_process_connack(const char *data, int data_len) {
	char *ptr = (char *)data;

	if (data_len != 4) {
		return ERR_MQTT_MSG_CONNACK_LENGTH;
	}

	if ((ptr[0]&MQTT_GET_MSG_TYPE) != MQTT_MSG_TYPE_CONNACK) {
		return ERR_MQTT_MSG_CONNACK_TYPE;
	}

	switch (ptr[3]) {
	case MQTT_CONNACK_ACCEPTED:
		return 0;
	case MQTT_CONNACK_REFUSED_VERSION:
		return MQTT_CONNACK_REFUSED_VERSION;
	case MQTT_CONNACK_REFUSED_ID:
		return ERR_MQTT_MSG_CONNACK_ID;
	case MQTT_CONNACK_REFUSED_BROKER:
		return ERR_MQTT_MSG_CONNACK_BROKER;
	case MQTT_CONNACK_REFUSED_BAD_USER_PASSWORD:
		return ERR_MQTT_MSG_CONNACK_LOGIN;
	case MQTT_CONNACK_REFUSED_NOT_AUTHORIZED:
		return ERR_MQTT_MSG_CONNACK_NOT_AUTHORIZED;
	default:
		return ERR_MQTT_MSG_CONNACK_UNKNOWN_RETURN;
	}
}

int _mqtt_msg_build_disconnect(char **data, int *data_len) {
	char *msg;
	if ((msg = (char *)_memory_malloc(2)) == NULL) {
		return ERR_TR50_MALLOC;
	}
	msg[0] = MQTT_MSG_TYPE_DISCONNECT;
	msg[1] = 0;
	*data = msg;
	*data_len = 2;
	return 0;
}

int _mqtt_msg_build_ping(char **data, int *data_len) {
	char *msg;
	if ((msg = (char *)_memory_malloc(2)) == NULL) {
		return ERR_TR50_MALLOC;
	}
	msg[0] = MQTT_MSG_TYPE_PINGREQ;
	msg[1] = 0;
	*data = msg;
	*data_len = 2;
	return 0;
}

int _mqtt_msg_build_publish(const char *topic, int qos, int retain, unsigned short msg_id, const char *payload, int payload_len, char **data, int *data_len) {
	int remaining_len = 0;
	int topic_len = 0;
	int fixed_header_len = 0;
	int total_len = 0;
	char *msg, *ptr;
	unsigned short msg_id_swap;

	topic_len = strlen(topic);
	remaining_len += topic_len + 2;

	if (qos > 0) {
		remaining_len += 2;    // message id
	}

	remaining_len += payload_len;

	MQTT_CALC_FHEADER_LENGTH(remaining_len, fixed_header_len);

	total_len = fixed_header_len + remaining_len;

	if ((msg = (char *)_memory_malloc(total_len)) == NULL) {
		return ERR_TR50_MALLOC;
	}
	ptr = msg;

	// set byte1
	*ptr = 0x00 | MQTT_MSG_TYPE_PUBLISH;
	if (qos > 0) {
		*ptr |= MQTT_OPT_QOS_1;
	}
	if (retain) {
		*ptr |= MQTT_OPT_RETAIN;
	}
	++ptr;

	// Encode the message length
	_mqtt_encode_fixed_header_len(remaining_len, ptr);
	ptr = msg + fixed_header_len;

	// Variable header
	// Add the topic name
	_mqtt_encode_string(ptr, topic, topic_len);
	ptr += topic_len + 2;

	// Add message id
	if (qos > 0) {
		msg_id_swap = swap16(msg_id);
		_memory_memcpy(ptr, &msg_id_swap, 2);
		ptr += 2;
	}

	// Add payload
	_memory_memcpy(ptr, (void *)payload, payload_len);

	*data = msg;
	*data_len = total_len;
	return 0;
}

int _mqtt_msg_build_puback(unsigned short msg_id, int qos, char **data, int *data_len) {
	unsigned short msg_id_swap;
	char *msg;
	if ((msg = (char *)_memory_malloc(4)) == NULL) {
		return ERR_TR50_MALLOC;
	}
	msg[0] = MQTT_MSG_TYPE_PUBACK;
	msg[1] = 0x2;

	msg_id_swap = swap16(msg_id);
	_memory_memcpy(msg + 2, &msg_id_swap, 2);

	*data = msg;
	*data_len = 4;
	return 0;
}

int mqtt_msg_process_puback(const char *data, int data_len, unsigned short *msg_id) {
	if (data_len != 4) {
		return ERR_MQTT_MSG_PUBACK_LENGTH;
	}

	if ((data[0]&MQTT_GET_MSG_TYPE) != MQTT_MSG_TYPE_PUBACK) {
		return ERR_MQTT_MSG_PUBACK_TYPE;
	}

	_memory_memcpy(msg_id, (void *)(data + 2), 2);

	*msg_id = swap16(*msg_id);

	return 0;
}

int mqtt_msg_process_suback(const char *data, int data_len, unsigned short *msg_id, int *qos) {
	if (data_len != 5) {
		return ERR_MQTT_MSG_SUBACK_LENGTH;
	}

	if ((data[0] & MQTT_GET_MSG_TYPE) != MQTT_MSG_TYPE_SUBACK) {
		return ERR_MQTT_MSG_SUBACK_TYPE;
	}

	_memory_memcpy(msg_id, (void *)(data + 2), 2);

	*msg_id = swap16(*msg_id);

	*qos = data[4];
	if (*qos & MQTT_OPT_QOS_FAILURE) return ERR_MQTT_MSG_SUBACK_QOS_FAILURE;
	return 0;
}

int mqtt_msg_process_unsuback(const char *data, int data_len, unsigned short *msg_id) {
	if (data_len != 4) {
		return ERR_MQTT_MSG_UNSUBACK_LENGTH;
	}

	if ((data[0] & MQTT_GET_MSG_TYPE) != MQTT_MSG_TYPE_UNSUBACK) {
		return ERR_MQTT_MSG_UNSUBACK_TYPE;
	}

	_memory_memcpy(msg_id, (void *)(data + 2), 2);

	*msg_id = swap16(*msg_id);
	return 0;
}

int mqtt_msg_process_publish(const char *data, int data_len, int *qos, unsigned short *msg_id, char **topic, char **payload, int *payload_len) {
	int ret, encoded_size, decoded_len, str_len;
	char *ptr = (char *)data;
	if (data_len <= 2) {
		return ERR_MQTT_MSG_PUBLISH_LENGTH;
	}

	if ((data[0]&MQTT_GET_MSG_TYPE) != MQTT_MSG_TYPE_PUBLISH) {
		return ERR_MQTT_MSG_PUBLISH_TYPE;
	}
	// get remaining_length
	if ((ret = _mqtt_decode_header_length((char *)data + 1, data_len - 1, &encoded_size, &decoded_len)) != 0) {
		return ERR_MQTT_MSG_PUBLISH_DECODE_HDR_LEN;
	}

	// get qos
	if (*ptr & MQTT_OPT_QOS_1) {
		*qos = 1;
	} else if (*ptr& MQTT_OPT_QOS_2) {
		*qos = 2;
	} else {
		*qos = 0;
	}
	++ptr;

	// skip encoded_size
	ptr += encoded_size;

	// get topic
	_mqtt_decode_string(ptr, &str_len, topic);
	ptr += str_len + 2;
	decoded_len -= (str_len + 2);

	// get message id
	if (*qos > 0) {
		_memory_memcpy(msg_id, ptr, 2);
		*msg_id = swap16(*msg_id);
		ptr += 2;
		decoded_len -= 2;
	}

	// get payload
	*payload = (char *)_memory_clone(ptr, decoded_len);
	*payload_len = decoded_len;
	return 0;
}

int _mqtt_msg_build_subscribe(const char *topic, int qos, unsigned short msg_id, char **data, int *data_len) {
	int remaining_len = 0;
	int topic_len = 0;
	int fixed_header_len = 0;
	int total_len = 0;
	char *msg, *ptr;
	unsigned short msg_id_swap;

	topic_len = strlen(topic);
	remaining_len = 2 + 2 + topic_len + 1;  //2 bytes msg_id, 2 bytes topic len, topic len, 1 byte QOS

	MQTT_CALC_FHEADER_LENGTH(remaining_len, fixed_header_len);

	total_len = fixed_header_len + remaining_len;

	if ((msg = (char*)_memory_malloc(total_len)) == NULL) return ERR_TR50_MALLOC;
	_memory_memset(msg, 0, total_len);
	ptr = msg;

	// set byte1
	*ptr = 0x02 | MQTT_MSG_TYPE_SUBSCRIBE;
	++ptr;

	// Encode the message length
	_mqtt_encode_fixed_header_len(remaining_len, ptr);
	ptr = msg + fixed_header_len;

	msg_id_swap = swap16(msg_id);
	_memory_memcpy(ptr, &msg_id_swap, 2);
	ptr += 2;

	// Variable header
	// Add the topic name
	_mqtt_encode_string(ptr, topic, topic_len);
	ptr += topic_len + 2;
	*ptr = qos & 0x3;
	*data = msg;
	*data_len = total_len;
	return 0;
}

int _mqtt_msg_build_unsubscribe(const char *topic, unsigned short msg_id, char **data, int *data_len) {
	int remaining_len = 0;
	int topic_len = 0;
	int fixed_header_len = 0;
	int total_len = 0;
	char *msg, *ptr;
	unsigned short msg_id_swap;

	topic_len = strlen(topic);
	remaining_len = 2 + 2 + topic_len;  //2 bytes msg_id, 2 bytes topic len, topic len

	MQTT_CALC_FHEADER_LENGTH(remaining_len, fixed_header_len);

	total_len = fixed_header_len + remaining_len;

	if ((msg = (char*)_memory_malloc(total_len)) == NULL) return ERR_TR50_MALLOC;
	_memory_memset(msg, 0, total_len);
	ptr = msg;

	// set byte1
	*ptr = 0x02 | MQTT_MSG_TYPE_UNSUBSCRIBE;
	++ptr;

	// Encode the message length
	_mqtt_encode_fixed_header_len(remaining_len, ptr);
	ptr = msg + fixed_header_len;

	msg_id_swap = swap16(msg_id);
	_memory_memcpy(ptr, &msg_id_swap, 2);
	ptr += 2;

	// Variable header
	// Add the topic name
	_mqtt_encode_string(ptr, topic, topic_len);
	ptr += topic_len + 2;
	*data = msg;
	*data_len = total_len;
	return 0;
}

int mqtt_msg_cmd_get(const char *data) {
	if ((data[0]&MQTT_GET_MSG_TYPE) == MQTT_MSG_TYPE_CONNACK) {
		return MQTT_MSG_TYPE_CONNACK;
	}
	if ((data[0]&MQTT_GET_MSG_TYPE) == MQTT_MSG_TYPE_PUBLISH) {
		return MQTT_MSG_TYPE_PUBLISH;
	}
	if ((data[0]&MQTT_GET_MSG_TYPE) == MQTT_MSG_TYPE_PUBACK) {
		return MQTT_MSG_TYPE_PUBACK;
	}
	if ((data[0]&MQTT_GET_MSG_TYPE) == MQTT_MSG_TYPE_PUBREC) {
		return MQTT_MSG_TYPE_PUBREC;
	}
	if ((data[0]&MQTT_GET_MSG_TYPE) == MQTT_MSG_TYPE_PUBCOMP) {
		return MQTT_MSG_TYPE_PUBCOMP;
	}
	if ((data[0] & MQTT_GET_MSG_TYPE) == MQTT_MSG_TYPE_SUBACK) {
		return MQTT_MSG_TYPE_SUBACK;
	}
	if ((data[0] & MQTT_GET_MSG_TYPE) == MQTT_MSG_TYPE_UNSUBACK) {
		return MQTT_MSG_TYPE_UNSUBACK;
	}
	if ((data[0]&MQTT_GET_MSG_TYPE) == MQTT_MSG_TYPE_UNSUBACK) {
		return MQTT_MSG_TYPE_UNSUBACK;
	}
	if ((data[0]&MQTT_GET_MSG_TYPE) == MQTT_MSG_TYPE_PINGRESP) {
		return MQTT_MSG_TYPE_PINGRESP;
	}
	return MQTT_MSG_TYPE_UNKNOWN;

}
