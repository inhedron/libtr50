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





#include <tr50/util/blob.h>

int _blob_create(void **handle, int initial_length) {
	_BLOB *blob;
	if ((handle == NULL)) {
		return ERR_TR50_PARMS;
	}
	*handle = NULL;

	if ((blob = _memory_malloc(sizeof(_BLOB))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(blob, 0, sizeof(_BLOB));

	if (initial_length <= 0) {
		blob->size = BLOB_DEFAULT_SIZE;
	} else {
		blob->size = initial_length;
	}
	if ((blob->buffer = _memory_malloc(sizeof(char) * blob->size)) == NULL) {
		_memory_free(blob);
		return ERR_TR50_MALLOC;
	}
	blob->unused = blob->size;
	_memory_memset(blob->buffer, 0, blob->size);

	*handle = blob;
	return 0;
}

void _blob_delete(void *handle) {
	if (handle == NULL) {
		return;
	}
	if (((_BLOB *)handle)->buffer != NULL) {
		_memory_free(((_BLOB *)handle)->buffer);
	}
	_memory_free(handle);
	return;
}

void _blob_delete_object(void *handle) {
	if (handle == NULL) {
		return;
	}
	_memory_free(handle);
	return;
}

int _blob_clear(void *handle) {
	_BLOB *blob = handle;
	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	_memory_memset(blob->buffer, 0, blob->size);
	blob->len = 0;
	blob->unused = blob->size;
	return 0;
}

void *_blob_get_buffer(void *handle) {
	if (handle == NULL) {
		return NULL;
	}
	return ((_BLOB *)handle)->buffer;
}

int _blob_get_length(void *handle) {
	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}
	return ((_BLOB *)handle)->len;
}
int _blob_append(void *handle, const char *data, int data_len) {
	_BLOB *blob = handle;
	int ret;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if ((ret = _blob_grow(blob, data_len)) < 0) {
		return ret;
	}

	if (data == NULL) {
		_memory_memset(blob->buffer + blob->len, 0, data_len);
	} else {
		_memory_memcpy(blob->buffer + blob->len, (void *)data, data_len);
	}
	blob->len += data_len;
	blob->unused -= data_len;
	return 0;
}


int _blob_grow(_BLOB *blob, int additional_bytes_needed) {
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
