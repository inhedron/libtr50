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

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ils/logging/log.h>

#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/platform.h>
#include <tr50/util/time.h>

#define LOG_BUFFER_SIZE		1024

int g_log_filter_level = LOG_TYPE_IMPORTANT_INFO;
void *g_tr50_util_logger = NULL;

void log_filter_maximum_log_level(int level) {
	g_log_filter_level = level;
}

void _log_vsnprintf(char *buffer, int len, const char *msg, va_list args) {
#if defined (_WIN32)
	_vsnprintf(buffer, LOG_BUFFER_SIZE, msg, args);
#else
	vsnprintf(buffer, LOG_BUFFER_SIZE, msg, args);
#endif
}

void _log_this_internal(int type, const char *file, int line, const char *msg, va_list args) {
	char buffer[LOG_BUFFER_SIZE];

	if (!g_tr50_util_logger) {
		log_create(&g_tr50_util_logger, "tr50");
	}

	switch (type) {
	case LOG_TYPE_SHOULD_NOT_HAPPEN:
	case LOG_TYPE_NEED_INVESTIGATION:
		if (log_is_warn_enabled(g_tr50_util_logger)) {
			_log_vsnprintf(buffer, LOG_BUFFER_SIZE, msg, args);
			lwarn(g_tr50_util_logger, buffer);
		}
		break;
	case LOG_TYPE_IMPORTANT_INFO:
		if (log_is_info_enabled(g_tr50_util_logger)) {
			_log_vsnprintf(buffer, LOG_BUFFER_SIZE, msg, args);
			linfo(g_tr50_util_logger, buffer);
		}
		break;
	case LOG_TYPE_DEBUG:
		if (log_is_debug_enabled(g_tr50_util_logger)) {
			_log_vsnprintf(buffer, LOG_BUFFER_SIZE, msg, args);
			ldebug(g_tr50_util_logger, buffer);
		}
		break;
	case LOG_TYPE_LOW_LEVEL:
		if (log_is_debug4_enabled(g_tr50_util_logger)) {
			_log_vsnprintf(buffer, LOG_BUFFER_SIZE, msg, args);
			ldebug4(g_tr50_util_logger, buffer);
		}
		break;
	}
	buffer[LOG_BUFFER_SIZE - 1] = 0;
}

void _log_this(int type, const char *file, int line, const char *msg, ...) {
	va_list args;

	va_start(args, msg);
	_log_this_internal(type, file, line, msg, args);
	va_end(args);
}

#define LOG_RECURRING_ENTRY_MAX		16

typedef struct {
	const char *file;
	int line;
	char message[LOG_BUFFER_SIZE + 1];

	int is_used;
	int counter;
	int last_called_timestamp;
	int last_logged_timestamp;
} _LOG_RECURRING_ENTRY;

_LOG_RECURRING_ENTRY g_log_recurring_entries[LOG_RECURRING_ENTRY_MAX];

void _log_this_recurring_internal(int type, const char *file, int line, int minutes_between_logs, int check_for_changes, const char *msg, va_list args) {
	int i;
	char buffer[LOG_BUFFER_SIZE];
	_LOG_RECURRING_ENTRY *oldest = NULL;
	int now = _time_now_in_sec();

	_log_vsnprintf(buffer, LOG_BUFFER_SIZE, msg, args);

	// find in array, if not found, replace the oldest one, and print old one if greater than 1
	for (i = 0; i < LOG_RECURRING_ENTRY_MAX; ++i) {
		_LOG_RECURRING_ENTRY *entry = &g_log_recurring_entries[i];
		if (entry->file == file && entry->line == line) {
			if (check_for_changes && strncmp(entry->message, buffer, LOG_BUFFER_SIZE) != 0) { // message changed
				_log_this(type, file, line, "Recurring updated from [%d]: %s", entry->counter, entry->message);

				strncpy(entry->message, buffer, LOG_BUFFER_SIZE);

				entry->last_logged_timestamp = now;
				entry->last_called_timestamp = now;
				entry->counter = 0;

				_log_this(type, file, line, "Recurring updated to: %s", entry->message);
				return;
			}

			// if dont need to print, increase counter
			++entry->counter;
			entry->last_called_timestamp = now;
			if (entry->last_logged_timestamp + minutes_between_logs * 60 < now) {
				_log_this(type, file, line, "Recurring[%d]: %s", entry->counter, entry->message);
				entry->last_logged_timestamp = now;
				entry->counter = 0;
			}
			return;
		}
	}
	
	// if not found, add to first empty slot then print
	for (i = 0; i < LOG_RECURRING_ENTRY_MAX; ++i) {
		_LOG_RECURRING_ENTRY *entry = &g_log_recurring_entries[i];
		if (!entry->is_used) {
			entry->file = file;
			entry->line = line;
			strncpy(entry->message, buffer, LOG_BUFFER_SIZE);

			entry->is_used = 1;
			entry->counter = 1;
			entry->last_called_timestamp = now;
			entry->last_logged_timestamp = now;
			linfo(g_tr50_util_logger, buffer);
			return;
		}
		if (!oldest || oldest->last_called_timestamp > entry->last_called_timestamp) {
			oldest = entry;
		}
	}
	_log_this(type, file, line, "Recurring released[%d]: %s", oldest->counter, oldest->message);
	oldest->file = file;
	oldest->line = line;
	strncpy(oldest->message, buffer, LOG_BUFFER_SIZE);

	oldest->counter = 1;
	oldest->last_called_timestamp = now;
	oldest->last_logged_timestamp = now;
	_log_this(type, file, line, "Recurring started: %s", oldest->message);
}

void log_recurring(int type, const char *filename, int line, int minutes_between_logs, int check_for_changes, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	_log_this_recurring_internal(type, filename, line, minutes_between_logs, check_for_changes, msg, args);
	va_end(args);

}

char *_log_hexdump(const char *msg, const void *data, int len) {
	unsigned char *buf = (unsigned char *)data;
	int indx;
	int ix2;
	char *buffer;
	int buffer_len;
	int buffer_size;

	if (data == NULL) {
		return NULL;
	}

	buffer_size = sizeof(char) * (len / 16) * 80 + 160 + strlen(msg);
	if ((buffer = _memory_malloc(buffer_size)) == NULL) {
		return NULL;
	}
	buffer_len = 0;

	buffer_len += snprintf(buffer, buffer_size, "%s - HEXDUMP AT [0x%p] (%d bytes)\n", msg, data, len);

	for (indx = 0; indx < len; ++indx) {
		if ((indx % 16) == 0) {
			buffer_len += snprintf(buffer + buffer_len, buffer_size - buffer_len, "  %4.4x:", indx);
		}
		buffer_len += snprintf(buffer + buffer_len, buffer_size - buffer_len, " %2.2x", buf[indx]);
		if ((indx % 16) == 7) {
			buffer[buffer_len++] = ' ';
			buffer[buffer_len++] = '-';
		} else if ((indx % 16) == 15) {
			buffer[buffer_len++] = ' ';
			buffer[buffer_len++] = ' ';
			for (ix2 = indx & -16; ix2 <= indx; ++ix2) {
				buffer[buffer_len++] = isprint(buf[ix2]) ? buf[ix2] : '.';
			}
			buffer[buffer_len++] = '\n';
		}
	}

	if ((indx % 16) != 0) {
		buffer_len += snprintf(buffer + buffer_len, buffer_size - buffer_len, "%*s  ", ((16 - (indx % 16)) * 3) + ((1 - ((indx % 16) / 8)) * 2), "");
		for (ix2 = indx & -16; ix2 < indx; ++ix2) {
			buffer[buffer_len++] = isprint(buf[ix2]) ? buf[ix2] : '.';
		}
		buffer[buffer_len] = '\0';
	} else {
		buffer[buffer_len - 1] = '\0';
	}

	return buffer;
}

void log_hexdump(int type, const char *msg, const char *data, int len) {
	switch (type) {
	case LOG_TYPE_SHOULD_NOT_HAPPEN:
	case LOG_TYPE_NEED_INVESTIGATION:
		if (log_is_warn_enabled(g_tr50_util_logger)) {
			char *dump = _log_hexdump(msg, data, len);
			lwarn(g_tr50_util_logger, dump);
			_memory_free(dump);
		}
		break;
	case LOG_TYPE_IMPORTANT_INFO:
		if (log_is_info_enabled(g_tr50_util_logger)) {
			char *dump = _log_hexdump(msg, data, len);
			linfo(g_tr50_util_logger, dump);
			_memory_free(dump);
		}
		break;
	case LOG_TYPE_DEBUG:
		if (log_is_debug_enabled(g_tr50_util_logger)) {
			char *dump = _log_hexdump(msg, data, len);
			ldebug(g_tr50_util_logger, dump);
			_memory_free(dump);
		}
		break;
	case LOG_TYPE_LOW_LEVEL:
		if (log_is_debug4_enabled(g_tr50_util_logger)) {
			char *dump = _log_hexdump(msg, data, len);
			ldebug4(g_tr50_util_logger, dump);
			_memory_free(dump);
		}
		break;
	}
}

#if defined(_NO_VA_ARGS)
void log_should_not_happen(const char *msg, ...) {
	va_list args;
	if(msg==NULL) return;
	va_start(args,msg);
	_log_this_internal(LOG_TYPE_SHOULD_NOT_HAPPEN,"NA",0,msg,args);
	va_end(args);
}
void log_need_investigation(const char *msg, ...) {
	va_list args;
	if(msg==NULL) return;
	va_start(args,msg);
	_log_this_internal(LOG_TYPE_NEED_INVESTIGATION,"NA",0,msg,args);
	va_end(args);
}
void log_important_info(const char *msg, ...) {
	va_list args;
	if(msg==NULL) return;
	va_start(args,msg);
	_log_this_internal(LOG_TYPE_IMPORTANT_INFO,"NA",0,msg,args);
	va_end(args);
}
void log_debug(const char *msg, ...) {
	va_list args;
	if(msg==NULL) return;
	va_start(args,msg);
	_log_this_internal(LOG_TYPE_DEBUG,"NA",0,msg,args);
	va_end(args);
}
void log_low_level(const char *msg, ...) {
	va_list args;
	if(msg==NULL) return;
	va_start(args,msg);
	_log_this_internal(LOG_TYPE_LOW_LEVEL,"NA",0,msg,args);
	va_end(args);
}

#endif
