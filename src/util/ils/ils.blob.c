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

#include <stdarg.h>
#include <string.h>

#include <ils/internal/platform.h>

#include <tr50/error.h>

#include <tr50/util/memory.h>

#define BLOB_DEFAULT_SIZE 1024

typedef struct {
	char *buffer;
	int size;
	int len;
	int unused;
	int array_length;
	int array_cur;
} _BLOB;

int _blob_grow_size(_BLOB *blob, int additional_bytes_needed);

int _blob_format_append(void *handle, const char *msg, ...) {
	va_list pArgs;
	_BLOB *blob = handle;
	int format_len = 0;
	int vsnl = -1;
#if defined(_WIN32)
	char *loc = NULL;
	const char *cur = msg;
	char *msg2 = NULL;
	int msglen;
#endif

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (msg == NULL) {
		return ERR_TR50_PARMS;
	}

	va_start(pArgs, msg);

#ifdef _UNIX

	vsnl = vsnprintf(blob->buffer + blob->len, blob->unused, msg, pArgs);

	if (vsnl == -1) {
		do {
			_blob_grow_size(blob, blob->size);
			vsnl = vsnprintf(blob->buffer + blob->len, blob->unused, msg, pArgs);
		} while (vsnl == -1);
	} else if (vsnl >= blob->unused) {
		_blob_grow_size(blob, vsnl + 1);
		vsnl = vsnprintf(blob->buffer + blob->len, blob->unused, msg, pArgs);
	}

	format_len = vsnl;

#elif _WIN32

	msglen = strlen(msg);

	while ((loc = strstr(cur, "%ll")) != NULL) {
		if (msg2 == NULL) {
			if ((msg2 = _memory_malloc(msglen + 16)) == NULL) {
				return ERR_TR50_MALLOC;
			}
			_memory_memset(msg2, 0, msglen + 16);
		}
		strncat(msg2, cur, loc - cur);
		strncat(msg2, "%I64", msglen + 16 - strlen(msg2));
		cur = loc + 1;
	}

	if (msg2 != NULL) {
		strncat(msg2, cur + 2, msglen + 16 - strlen(msg2));
		while (vsnl == -1) {
			vsnl = _vsnprintf(blob->buffer + blob->len, blob->unused - 1, msg2, pArgs);
			if (vsnl == -1) {
				_blob_grow_size(blob, blob->size * 2);
			}
		}
		_memory_free(msg2);
	} else {
		while (vsnl == -1) {
			vsnl = _vsnprintf(blob->buffer + blob->len, blob->unused - 1, msg, pArgs);
			if (vsnl == -1) {
				_blob_grow_size(blob, blob->size * 2);
			}
		}
	}

	format_len = strlen(blob->buffer + blob->len);

#endif

	va_end(pArgs);
	blob->len += format_len;
	blob->unused -= format_len;

	return 0;
}

int _blob_grow_size(_BLOB *blob, int additional_bytes_needed) {
	char *tmp_buffer;
	int new_size;

	if (blob == NULL) {
		return ERR_TR50_PARMS;
	}

	if (additional_bytes_needed < blob->unused) {
		return 0;
	} else {
		if (additional_bytes_needed - blob->unused > blob->size) {
			new_size = additional_bytes_needed + blob->len;
		} else {
			new_size = blob->size * 2;
		}
	}

	if ((tmp_buffer = _memory_realloc(blob->buffer, new_size)) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(tmp_buffer + blob->size, 0, new_size - blob->size);

	blob->size = new_size;
	blob->unused = new_size - blob->len;
	blob->buffer = tmp_buffer;

	return 0;
}
