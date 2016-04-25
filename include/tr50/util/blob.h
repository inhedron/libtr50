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
#include <stdio.h>
#include <tr50/error.h>
#include <tr50/util/log.h>
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

int _blob_create(void **handle, int initial_length);
void _blob_delete(void *handle);
void _blob_delete_object(void *handle);
int _blob_clear(void *handle);
void *_blob_get_buffer(void *handle);
int _blob_get_length(void *handle);
int _blob_format_append(void *handle, const char *msg, ...);
int _blob_append(void *handle, const char *data, int data_len);
int _blob_grow(_BLOB *blob, int additional_bytes_needed);
